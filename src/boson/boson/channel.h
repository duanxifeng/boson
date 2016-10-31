#ifndef BOSON_CHANNEL_H_
#define BOSON_CHANNEL_H_

#include <list>
#include <memory>
#include <mutex>
#include "boson/semaphore.h"
#include "engine.h"
#include "internal/routine.h"
#include "internal/thread.h"

namespace boson {

template <class ContentType, std::size_t Size>
class channel_impl {
  static_assert(0 < Size, "Boson channels do not support zero size.");
  template <class Content, std::size_t InSize, class Func>
  friend class event_channel_read_storage;
  template <class Content, std::size_t InSize, class Func>
  friend class event_channel_write_storage;

  std::array<ContentType, Size> buffer_;
  std::atomic<size_t> head_;
  std::atomic<size_t> tail_;

  // Waiting lists
  boson::shared_semaphore readers_slots_;
  boson::shared_semaphore writer_slots_;

 public:
  channel_impl() : buffer_{}, head_{0}, tail_{0}, readers_slots_(0), writer_slots_(Size) {
  }

  ~channel_impl() {
    // delete queue_;
  }

  void consume_write(thread_id tid, ContentType value) {
    size_t head = head_.fetch_add(1, std::memory_order_acq_rel);
    buffer_[head % Size] = std::move(value);
    readers_slots_.post();
  }

  void consume_read(thread_id tid, ContentType& value) {
    size_t tail = tail_.fetch_add(1, std::memory_order_acq_rel);
    value = std::move(buffer_[tail % Size]);
    writer_slots_.post();
  }

  /**
   * Write an element in the channel
   *
   * Returns false only if the channel is closed.
   */
  bool write(thread_id tid, ContentType value, int timeout_ms = -1) {
    bool ticket = writer_slots_.wait(timeout_ms = -1);
    if (!ticket)
      return false;
    consume_write(tid, value);
    return true;
  }

  bool read(thread_id tid, ContentType& value, int  timeout_ms = -1) {
    bool ticket = readers_slots_.wait(timeout_ms);
    if (!ticket)
      return false;
    consume_read(tid, value);
    return true;
  }
};

/**
 * Specialization for the channel containing nothing
 */
template <std::size_t Size>
class channel_impl<std::nullptr_t,Size> {
  static_assert(0 < Size, "Boson channels do not support zero size.");
  template <class Content, std::size_t InSize, class Func>
  friend class event_channel_read_storage;
  template <class Content, std::size_t InSize, class Func>
  friend class event_channel_write_storage;

  using ContentType = std::nullptr_t;

  // Waiting lists
  boson::semaphore readers_slots_;
  boson::semaphore writer_slots_;

 public:
  channel_impl() : readers_slots_(0), writer_slots_(Size) {
  }

  ~channel_impl() {
  }

  void consume_write(thread_id tid, ContentType value) {
    readers_slots_.post();
  }

  void consume_read(thread_id tid, ContentType& value) {
    value = nullptr;
    writer_slots_.post();
  }

  /**
   * Write an element in the channel
   *
   * Returns false only if the channel is closed.
   */
  bool write(thread_id tid, ContentType value, int timeout_ms = -1) {
    bool ticket = writer_slots_.wait(timeout_ms = -1);
    if (!ticket)
      return false;
    consume_write(tid, value);
    return true;
  }

  bool read(thread_id tid, ContentType& value, int  timeout_ms = -1) {
    bool ticket = readers_slots_.wait(timeout_ms);
    if (!ticket)
      return false;
    consume_read(tid, value);
    return true;
  }
};

/**
 * Channel use interface
 *
 * The user is supposed ot use and copy channel objects
 * and not directly use the implementation. channel objects must
 * never be transmitted to new routines through reference
 * but only by copy.
 */
template <class ContentType, std::size_t Size>
class channel {
  template <class Content, std::size_t InSize, class Func>
  friend class event_channel_read_storage;
  template <class Content, std::size_t InSize, class Func>
  friend class event_channel_write_storage;
  using value_t = ContentType;
  using impl_t = channel_impl<value_t, Size>;

  std::shared_ptr<impl_t> channel_;
  thread_id thread_id_{0};

  thread_id get_id() {
    if (!thread_id_) {
      internal::thread* this_thread = internal::current_thread();
      assert(this_thread);
      thread_id_ = this_thread->id();
    }
    return thread_id_;
  }

 public:
  using value_type = ContentType;
  static constexpr size_t size = Size;

  /**
   * Channel construction determines its behavior
   *
   * == 0 means sync channel
   * > 0 means channel of size capacity
   */
  channel() : channel_{new impl_t} {
  }
  channel(channel const&) = default;
  channel(channel&&) = default;
  channel& operator=(channel const&) = default;
  channel& operator=(channel&&) = default;

  //template <class... Args>
  //inline bool write(Args&&... args) {
    //return channel_->write(get_id(), std::forward<Args>(args)...);
  //}
  inline void consume_write(ContentType value) {
    channel_->consume_write(get_id(), std::move(value));
  }

  inline void consume_read(ContentType& value) {
    channel_->consume_read(get_id(), value);
  }

  inline bool write(ContentType value, int timeout_ms = -1) {
    return channel_->write(get_id(), std::move(value), timeout_ms);
  }

  inline bool read(ContentType& value, int timeout_ms = -1) {
    return channel_->read(get_id(), value, timeout_ms);
  }
};

template <class ContentType, std::size_t Size, class ValueType>
inline auto operator << (channel<ContentType, Size>& channel, ValueType&& value) 
-> typename std::enable_if<std::is_convertible<ValueType,ContentType>::value, bool>::type
{
  return channel.write(static_cast<ContentType>(std::forward<ValueType>(value)));
}

template <class ContentType, std::size_t Size>
inline auto operator >> (channel<ContentType, Size>& channel, ContentType& value) 
{
  return channel.read(value);
}

}  // namespace bosn

#endif  // BOSON_CHANNEL_H_
