#ifndef BOSON_QUEUES_WFQUEUE_H_
#define BOSON_QUEUES_WFQUEUE_H_
#include <unistd.h>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <list>
#include <mutex>
#include <type_traits>
#include <utility>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
extern "C" {
#include "wfqueue/queue.h"
}
#pragma GCC diagnostic pop

namespace boson {
namespace queues {

class simple_wfqueue {
  std::list<void*> queue_;
  std::mutex mut_;

 public:
  simple_wfqueue(int nprocs_);
  simple_wfqueue(simple_wfqueue const&) = delete;
  simple_wfqueue(simple_wfqueue&&) = default;
  simple_wfqueue& operator=(simple_wfqueue const&) = delete;
  simple_wfqueue& operator=(simple_wfqueue&&) = default;
  void push(std::size_t proc_id, void* data);
  void* pop(std::size_t proc_id);
};

/**
 * Wfqueue is a wait-free MPMC concurrent queue
 *
 * @see https://github.com/chaoran/fast-wait-free-queue
 *
 * Algorithm from Chaoran Yang and John Mellor-Crummey
 * Currently, thei algorithms bugs here so we use LCRQ
 */
class base_wfqueue {
  queue_t* queue_;
  handle_t** hds_;
  int nprocs_;

  handle_t* get_handle(std::size_t proc_id);

 public:
  base_wfqueue(int nprocs);
  base_wfqueue(base_wfqueue const&) = delete;
  base_wfqueue(base_wfqueue&&) = default;
  base_wfqueue& operator=(base_wfqueue const&) = delete;
  base_wfqueue& operator=(base_wfqueue&&) = default;
  ~base_wfqueue();
  void push(std::size_t proc_id, void* data);
  void* pop(std::size_t proc_id);
};

};  // namespace queues
};  // namespace boson

#endif  // BOSON_QUEUES_WFQUEUE_H_
