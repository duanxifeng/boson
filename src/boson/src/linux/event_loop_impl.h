#ifndef BOSON_EVENTLOOPIMPL_H_
#define BOSON_EVENTLOOPIMPL_H_
#pragma once

#include <sys/epoll.h>
#include <vector>
#include "event_loop.h"
#include "memory/sparse_vector.h"

namespace boson {
using epoll_event_t = struct epoll_event;

/**
 * Event loop Linux implementation
 *
 * Refer to the event loop interface for member functions
 * meaning
 */
class event_loop_impl {

  enum class event_type {
    event_fd,
    read,
    write
  };

  struct event_data {
    int fd;
    event_type type;
    void* data;
  };

  event_handler& handler_;
  int loop_fd_{-1};
  memory::sparse_vector<event_data> events_data_;
  std::vector<epoll_event_t> events_;

 public:
  event_loop_impl(event_handler& handler);
  ~event_loop_impl();
  int register_event(void* data);
  void* unregister_event(int event_id);
  void send_event(int event);
  void request_read(int fd, void* data);
  void request_write(int fd, void* data);
  loop_end_reason loop(int max_iter = -1, int timeout_ms = -1);
};
}

#endif  // BOSON_EVENTLOOP_H_
