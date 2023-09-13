#pragma once

#include <utility>

#include "detail/noncopyable.hpp"
#include "detail/tcp_channel_t.hpp"
#include "rpc_core.hpp"

namespace asio_net {
namespace detail {

template <socket_type T>
class rpc_session_t : noncopyable, public std::enable_shared_from_this<rpc_session_t<T>> {
 public:
  explicit rpc_session_t(asio::io_context& io_context) : io_context_(io_context) {
    ASIO_NET_LOGD("rpc_session: %p", this);
  }

  ~rpc_session_t() {
    ASIO_NET_LOGD("~rpc_session: %p", this);
  }

 public:
  void init(std::weak_ptr<detail::tcp_channel_t<T>> ws) {
    tcp_session_ = std::move(ws);
    auto tcp_session = tcp_session_.lock();

    rpc = rpc_core::rpc::create();

    rpc->set_timer([this](uint32_t ms, rpc_core::rpc::timeout_cb cb) {
      auto timer = std::make_shared<asio::steady_timer>(io_context_);
      timer->expires_after(std::chrono::milliseconds(ms));
      auto tp = timer.get();
      tp->async_wait([timer = std::move(timer), cb = std::move(cb)](const std::error_code&) {
        cb();
      });
    });

    rpc->get_connection()->send_package_impl = [this](std::string data) {
      auto tcp_session = tcp_session_.lock();
      if (tcp_session) {
        tcp_session->send(std::move(data));
      } else {
        ASIO_NET_LOGW("tcp_session expired on sendPackage");
      }
    };

    // bind rpc_session lifecycle to tcp_session and end with on_close
    tcp_session->on_close = [this, rpc_session = this->shared_from_this()]() mutable {
      if (rpc_session->on_close) {
        rpc_session->on_close();
      }
      // post delay destroy rpc_session, ensure rpc.rsp() callback finish
      io_context_.post([rpc_session = std::move(rpc_session)] {});
      // clear tcp_session->on_close, avoid called more than once by close api
      tcp_session_.lock()->on_close = nullptr;
    };

    tcp_session->on_data = [this](std::string data) {
      rpc->get_connection()->on_recv_package(std::move(data));
    };
  }

  void close() {
    auto ts = tcp_session_.lock();
    if (ts) {
      ts->close();
    }
  }

 public:
  std::function<void()> on_close;

 public:
  std::shared_ptr<rpc_core::rpc> rpc;

 private:
  asio::io_context& io_context_;
  std::weak_ptr<detail::tcp_channel_t<T>> tcp_session_;
};

}  // namespace detail
}  // namespace asio_net
