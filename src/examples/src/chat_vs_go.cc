#include <iostream>
#include <chrono>
#include <set>
#include <string>
#include <atomic>
#include "boson/boson.h"
#include "boson/channel.h"
#include "boson/net/socket.h"
#include "boson/select.h"
#include "boson/shared_buffer.h"
#include "fmt/format.h"
#include "iostream"

using namespace boson;
using namespace std::chrono_literals;

void listen_client(int fd, channel<std::string, 5> msg_chan, channel<int, 5> close_chan) {
  std::array<char, 2048> buffer;
  for (;;) {
    ssize_t nread = boson::recv(fd, buffer.data(), buffer.size(), 0);
    if (nread <= 1) {
      close_chan << fd;
      return;
    }
    std::string message(buffer.data(), nread - 2);
    if (message == "quit") {
      close_chan << fd;
      return;
    }
    msg_chan << message;
  }
}

void handleNewConnections(boson::channel<int, 5> newConnChan) {
  int sockfd = net::create_listening_socket(8080);
  struct sockaddr_in cli_addr;
  socklen_t clilen = sizeof(cli_addr);
  for (;;) {
    int newFd = boson::accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
    if (0 <= newFd) {
      newConnChan << newFd;
    }
  }
}

void displayCounter(std::atomic<uint64_t>* counter) {
  for (;;) {
    ::printf("%lu\n",counter->exchange(0,std::memory_order_relaxed));
    boson::sleep(1000ms);
  }
}

int main(int argc, char *argv[]) {
  boson::run(1, []() {
    channel<int, 5> new_connection;
    channel<std::string, 5> messages;
    channel<int, 5> close_connection;

    std::atomic<uint64_t> counter {0};
    boson::start(displayCounter, &counter);

    boson::start(handleNewConnections, new_connection);

    // Create socket and list to connections

    // Main loop
    std::set<int> conns;
    for (;;) {
      int conn = 0;
      std::string message;
      select_any(                           //
          event_read(new_connection, conn,  //
                     [&](bool) {            //
                       conns.insert(conn);
                       start(listen_client, conn, messages, close_connection);
                     }),
          event_read(messages, message,
                     [&](bool) {  //
                       message += "\n";
                       for (auto fd : conns) {
                         boson::write(fd, message.data(), message.size());
                         counter.fetch_add(1,std::memory_order_relaxed);
                       }
                     }),
          event_read(close_connection, conn,
                     [&](bool) {  //
                       conns.erase(conn);
                       ::shutdown(conn, SHUT_WR);
                       ::close(conn);
                     }));
    };
  });
}
