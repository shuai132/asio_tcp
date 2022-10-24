#pragma once

#include "detail/noncopyable.hpp"
#include "tcp_channel.hpp"

namespace asio_net {

class tcp_client : public tcp_channel {
 public:
  explicit tcp_client(asio::io_context& io_context, PackOption pack_option = PackOption::DISABLE, uint32_t max_body_size = UINT32_MAX)
      : tcp_channel(socket_, pack_option_, max_body_size_), socket_(io_context), pack_option_(pack_option), max_body_size_(max_body_size) {}

  void open(const std::string& ip, const std::string& port) {
    auto resolver = std::make_unique<tcp::resolver>(socket_.get_executor());
    resolver->async_resolve(tcp::resolver::query(ip, port),
                            [this, resolver = std::move(resolver)](const asio::error_code& ec, const tcp::resolver::results_type& endpoints) {
                              if (!ec) {
                                do_connect(endpoints);
                              } else {
                                if (on_open_failed) on_open_failed(ec);
                              }
                            });
  }

 public:
  std::function<void()> on_open;
  std::function<void(std::error_code)> on_open_failed;

 private:
  void do_connect(const tcp::resolver::results_type& endpoints) {
    asio::async_connect(socket_, endpoints, [this](const std::error_code& ec, const tcp::endpoint&) {
      if (!ec) {
        if (on_open) on_open();
        do_read_start();
      } else {
        if (on_open_failed) on_open_failed(ec);
      }
    });
  }

 private:
  tcp::socket socket_;
  PackOption pack_option_;
  uint32_t max_body_size_;
};

}  // namespace asio_net
