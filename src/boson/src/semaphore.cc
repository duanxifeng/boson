#include "boson/semaphore.h"
#include <cassert>
#include "boson/engine.h"
#include "boson/logger.h"

using namespace std::chrono;

namespace boson {

semaphore::semaphore(int capacity)
    : waiters_{static_cast<int>(internal::current_thread()->get_engine().max_nb_cores())},
      counter_{capacity} {
}

semaphore::~semaphore() {
}

bool semaphore::pop_a_waiter(internal::thread* current) {
  using namespace internal;
  routine* current_routine = nullptr;
  int result = 1;
  if(0 < result) {
    auto waiter = static_cast<std::pair<internal::thread*,std::size_t>*>(waiters_.read(current->id()));
    if (waiter) {
      thread* managing_thread = waiter->first;
      managing_thread->push_command(
          current->id(), std::make_unique<thread_command>(
                             thread_command_type::schedule_waiting_routine,
                             std::make_pair(this,waiter->second)));
      delete waiter;
      return true;
    }
  }
  return true;
}

bool semaphore::wait(milliseconds timeout) {
  using namespace internal;
  int result = counter_.fetch_sub(1,std::memory_order_acquire);
  routine* current_routine = nullptr;
  while (result <= 0) {
    // We failed to get the semaphore, we have to suspend the routine
    thread* this_thread = internal::current_thread();
    assert(this_thread);
    routine* current_routine = this_thread->running_routine();
    current_routine->status_ = routine_status::wait_events;
    current_routine->start_event_round();
    current_routine->add_semaphore_wait(this);
    this_thread->context() = jump_fcontext(this_thread->context().fctx, this);
    if (current_routine->status_ == routine_status::timed_out) {
      current_routine->previous_status_ = routine_status::timed_out;
      current_routine->status_ = routine_status::running;
      return false;
    }
    result = counter_.fetch_sub(1,std::memory_order_acquire);
    current_routine->previous_status_ = routine_status::wait_events;
    current_routine->status_ = routine_status::running;
  }
  return true;
}

void semaphore::post() {
  using namespace internal;
  int result = counter_.fetch_add(1,std::memory_order_release);
  if (0 <= result) {
    // We may not gotten in the middle of a wait, so we cant avoid to try a pop
    pop_a_waiter(internal::current_thread());
  }
  // We do not yield, this is the wait purpose
}

}  // namespace boson
