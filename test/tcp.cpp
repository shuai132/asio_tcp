#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <thread>

#include "asio_net/tcp_client.hpp"
#include "asio_net/tcp_server.hpp"
#include "assert_def.h"
#include "log.h"

using namespace asio_net;

const uint16_t PORT = 6666;

int main(int argc, char** argv) {
  static uint32_t test_count_max = 10000;
  static uint32_t test_count_expect = 0;
  if (argc >= 2) {
    test_count_max = std::strtol(argv[1], nullptr, 10);
  }

  // server
  static std::atomic_bool pass_flag_session_close{false};
  std::thread([] {
    asio::io_context context;
    tcp_server server(context, PORT, tcp_config{.auto_pack = true});
    server.on_session = [](const std::weak_ptr<tcp_session>& ws) {
      LOG("on_session:");
      auto session = ws.lock();
      session->on_close = [] {
        LOG("session on_close:");
        pass_flag_session_close = true;
      };
      session->on_data = [ws](std::string data) {
        ASSERT(!ws.expired());
#ifndef ASIO_NET_DISABLE_ON_DATA_PRINT
        LOG("session on_data: %s", data.c_str());
#endif
        ws.lock()->send(std::move(data));
      };
    };
    server.start(true);
  }).detach();

  // client
  static std::atomic_bool pass_flag_client_close{false};
  std::thread([] {
    asio::io_context context;
    tcp_client client(context, tcp_config{.auto_pack = true});
    client.on_open = [&] {
      LOG("client on_open:");
      ASSERT(client.is_open);
      for (uint32_t i = 0; i < test_count_max; ++i) {
        client.send(std::to_string(i));
      }
    };
    client.on_data = [&](const std::string& data) {
      ASSERT(client.is_open);
#ifndef ASIO_NET_DISABLE_ON_DATA_PRINT
      LOG("client on_data: %s", data.c_str());
#endif
      ASSERT(std::to_string(test_count_expect++) == data);
      if (test_count_expect == test_count_max - 1) {
        client.close();
      }
    };
    client.on_close = [&] {
      ASSERT(!client.is_open);
      pass_flag_client_close = true;
      ASSERT(test_count_expect == test_count_max - 1);
      LOG("client on_close:");
      client.stop();
    };
    client.open("localhost", PORT);
    client.run();
  }).join();

  ASSERT(pass_flag_session_close);
  ASSERT(pass_flag_client_close);
  return EXIT_SUCCESS;
}
