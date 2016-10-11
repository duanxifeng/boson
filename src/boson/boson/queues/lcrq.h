#ifndef BOSON_QUEUES_LCRQ_H_
#define BOSON_QUEUES_LCRQ_H_
#include <unistd.h>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <list>
#include <type_traits>
#include <utility>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
#pragma GCC diagnostic pop

extern "C" { 
#include "wfqueue/align.h"
#include "wfqueue/primitives.h"
#include "wfqueue/hzdptr.h"
}

#define EMPTY ((void *) -1)
#ifndef LCRQ_RING_SIZE
#define LCRQ_RING_SIZE (1ull << 12)
#endif

namespace boson {
namespace queues {

class lcrq {




typedef struct RingNode {
  volatile uint64_t val;
  volatile uint64_t idx;
  uint64_t pad[14];
} RingNode DOUBLE_CACHE_ALIGNED;

typedef struct RingQueue {
  volatile int64_t head DOUBLE_CACHE_ALIGNED;
  volatile int64_t tail DOUBLE_CACHE_ALIGNED;
  struct RingQueue *next DOUBLE_CACHE_ALIGNED;
  RingNode array[LCRQ_RING_SIZE];
} RingQueue DOUBLE_CACHE_ALIGNED;

typedef struct {
  RingQueue * volatile head DOUBLE_CACHE_ALIGNED;
  RingQueue * volatile tail DOUBLE_CACHE_ALIGNED;
  int nprocs;
} queue_t;

typedef struct {
  RingQueue * next;
  hzdptr_t hzdptr;
} handle_t;

#define RING_SIZE LCRQ_RING_SIZE

//inline uint64_t node_index(uint64_t i) __attribute__ ((pure));
//inline uint64_t set_unsafe(uint64_t i) __attribute__ ((pure));
//inline uint64_t node_unsafe(uint64_t i) __attribute__ ((pure));
//inline uint64_t tail_index(uint64_t t) __attribute__ ((pure));
//inline int crq_is_closed(uint64_t t) __attribute__ ((pure));
//
inline void init_ring(RingQueue *r) {
  int i;

  for (i = 0; i < RING_SIZE; i++) {
    r->array[i].val = -1;
    r->array[i].idx = i;
  }

  r->head = r->tail = 0;
  r->next = NULL;
}

inline int is_empty(uint64_t v)   __attribute__ ((pure)) {
  return (v == (uint64_t)-1);
}


inline uint64_t node_index(uint64_t i) __attribute__ ((pure)){
  return (i & ~(1ull << 63));
}


inline uint64_t set_unsafe(uint64_t i) __attribute__ ((pure)){
  return (i | (1ull << 63));
}


inline uint64_t node_unsafe(uint64_t i) __attribute__ ((pure)){
  return (i & (1ull << 63));
}


inline uint64_t tail_index(uint64_t t) __attribute__ ((pure)){
  return (t & ~(1ull << 63));
}


inline int crq_is_closed(uint64_t t) __attribute__ ((pure)){
  return (t & (1ull << 63)) != 0;
}

void queue_init(queue_t * q, int nprocs)
{
  RingQueue *rq = (RingQueue*)align_malloc(PAGE_SIZE, sizeof(RingQueue));
  init_ring(rq);

  q->head = rq;
  q->tail = rq;
  q->nprocs = nprocs;
}

inline void fixState(RingQueue *rq) {

  while (1) {
    uint64_t t = rq->tail;
    uint64_t h = rq->head;

    if (rq->tail != t)
      continue;

    if (h > t) {
      if (CAS(&rq->tail, &t, h)) break;
      continue;
    }
    break;
  }
}

inline int close_crq(RingQueue *rq, const uint64_t t, const int tries) {
  uint64_t tt = t + 1;

  if (tries < 10)
    return CAS(&rq->tail, &tt, tt|(1ull<<63));
  else
    return BTAS(&rq->tail, 63);
}

void lcrq_put(queue_t * q, handle_t * handle, uint64_t arg) {
  int try_close = 0;

  while (1) {
    RingQueue *rq = (RingQueue*)hzdptr_setv(&q->tail, &handle->hzdptr, 0);
    RingQueue *next = rq->next;

    if (next != NULL) {
      CAS(&q->tail, &rq, next);
      continue;
    }

    uint64_t t = FAA(&rq->tail, 1);

    if (crq_is_closed(t)) {
      RingQueue * nrq;
alloc:
      nrq = handle->next;

      if (nrq == NULL) {
        nrq = (RingQueue*)align_malloc(PAGE_SIZE, sizeof(RingQueue));
        init_ring(nrq);
      }

      // Solo enqueue
      nrq->tail = 1;
      nrq->array[0].val = (uint64_t) arg;
      nrq->array[0].idx = 0;

      if (CAS(&rq->next, &next, nrq)) {
        CAS(&q->tail, &rq, nrq);
        handle->next = NULL;
        return;
      }
      continue;
    }

    RingNode* cell = &rq->array[t & (RING_SIZE-1)];

    uint64_t idx = cell->idx;
    uint64_t val = cell->val;

    if (is_empty(val)) {
      if (node_index(idx) <= t) {
        if ((!node_unsafe(idx) || rq->head < t) &&
            CAS2(cell, &val, &idx, arg, t)) {
          return;
        }
      }
    }

    uint64_t h = rq->head;

    if ((int64_t)(t - h) >= (int64_t)RING_SIZE &&
        close_crq(rq, t, ++try_close)) {
      goto alloc;
    }
  }

  hzdptr_clear(&handle->hzdptr, 0);
}

uint64_t lcrq_get(queue_t * q, handle_t * handle) {
  while (1) {
    RingQueue *rq = (RingQueue*)hzdptr_setv(&q->head, &handle->hzdptr, 0);
    RingQueue *next;

    uint64_t h = FAA(&rq->head, 1);

    RingNode* cell = &rq->array[h & (RING_SIZE-1)];

    uint64_t tt = 0;
    int r = 0;

    while (1) {

      uint64_t cell_idx = cell->idx;
      uint64_t unsafe = node_unsafe(cell_idx);
      uint64_t idx = node_index(cell_idx);
      uint64_t val = cell->val;

      if (idx > h) break;

      if (!is_empty(val)) {
        if (idx == h) {
          if (CAS2(cell, &val, &cell_idx, -1, (unsafe | h) + RING_SIZE))
            return val;
        } else {
          if (CAS2(cell, &val, &cell_idx, val, set_unsafe(idx))) {
            break;
          }
        }
      } else {
        if ((r & ((1ull << 10) - 1)) == 0)
          tt = rq->tail;

        // Optimization: try to bail quickly if queue is closed.
        int crq_closed = crq_is_closed(tt);
        uint64_t t = tail_index(tt);

        if (unsafe) { // Nothing to do, move along
          if (CAS2(cell, &val, &cell_idx, val, (unsafe | h) + RING_SIZE))
            break;
        } else if (t < h + 1 || r > 200000 || crq_closed) {
          if (CAS2(cell, &val, &idx, val, h + RING_SIZE)) {
            if (r > 200000 && tt > RING_SIZE)
              BTAS(&rq->tail, 63);
            break;
          }
        } else {
          ++r;
        }
      }
    }

    if (tail_index(rq->tail) <= h + 1) {
      fixState(rq);
      // try to return empty
      next = rq->next;
      if (next == NULL)
        return -1;  // EMPTY
      if (tail_index(rq->tail) <= h + 1) {
        if (CAS(&q->head, &rq, next)) {
          hzdptr_retire(&handle->hzdptr, rq);
        }
      }
    }
  }

  hzdptr_clear(&handle->hzdptr, 0);
}

void queue_register(queue_t * q, handle_t * th, int id)
{
  hzdptr_init(&th->hzdptr, q->nprocs, 1);
}

void enqueue(queue_t * q, handle_t * th, void * val)
{
  lcrq_put(q, th, (uint64_t) val);
}

void * dequeue(queue_t * q, handle_t * th)
{
  return (void *) lcrq_get(q, th);
}

//void queue_free(queue_t* q, int nprocs) {}





  //////////////



  queue_t* queue_;
  handle_t** hds_;
  int nprocs_;
  handle_t* get_handle(std::size_t proc_id);

 public:
  lcrq(int nprocs);
  lcrq(lcrq const&) = delete;
  lcrq(lcrq&&) = default;
  lcrq& operator=(lcrq const&) = delete;
  lcrq& operator=(lcrq&&) = default;
  ~lcrq();
  void write(std::size_t proc_id, void* data);
  void* read(std::size_t proc_id);
};

};  // namespace queues
};  // namespace boson

#endif  // BOSON_QUEUES_LCRQ_H_
