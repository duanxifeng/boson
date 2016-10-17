#ifndef BOSON_SYSCALLS_H_
#define BOSON_SYSCALLS_H_

#include <sys/socket.h>
#include <chrono>
#include <cstdint>
#include "system.h"

namespace boson {

static constexpr int code_panic = -100;
static constexpr int code_timeout = -101;

/**
 * Gives back control to the scheduler
 *
 * This is useful in CPU intensive routines not to block
 * other routines from be executed as well.
 */
void yield();

/**
 * Suspends the routine for the given duration
 */
void sleep(std::chrono::milliseconds duration);

/**
 * Suspends the routine until the fd is read for a syscall
 */
int wait_readiness(fd_t fd, bool read);

/**
 * Suspends the routine until the fd is read for read/recv/accept
 */
inline int wait_read_readiness(fd_t fd) {
  return wait_readiness(fd,true);
}

/**
 * Suspends the routine until the fd is read for write/send
 */
inline int wait_write_readiness(fd_t fd) {
  return wait_readiness(fd,false);
}

/**
 * Boson equivalent to POSIX read system call
 */
ssize_t read(fd_t fd, void *buf, size_t count);

/**
 * Boson equivalent to POSIX write system call
 */
ssize_t write(fd_t fd, const void *buf, size_t count);

socket_t accept(socket_t socket, sockaddr *address, socklen_t *address_len);
size_t send(socket_t socket, const void *buffer, size_t length, int flags);
ssize_t recv(socket_t socket, void *buffer, size_t length, int flags);

void fd_panic(int fd);

}  // namespace boson

#endif  // BOSON_SYSCALLS_H_
