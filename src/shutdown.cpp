#include "shutdown.hxx"

#include <atomic>
#include <csignal>
#include <mutex>
#include <thread>

namespace {
std::atomic<bool> shutdown_requested = false;
std::once_flag signal_thread_once;
sigset_t shutdown_signal_set;
} // namespace

void install_signal_handlers() {
  sigemptyset(&shutdown_signal_set);
  sigaddset(&shutdown_signal_set, SIGINT);
  sigaddset(&shutdown_signal_set, SIGTERM);

  // Block in this thread (and thus in subsequently created threads) so these
  // signals are received synchronously by the waiter thread below.
  pthread_sigmask(SIG_BLOCK, &shutdown_signal_set, nullptr);

  std::call_once(signal_thread_once, []() {
    std::thread([]() {
      int caught_signal = 0;
      while (sigwait(&shutdown_signal_set, &caught_signal) == 0) {
        shutdown_requested.store(true);
      }
    }).detach();
  });

  std::signal(SIGPIPE, SIG_IGN);
}

void request_shutdown() { shutdown_requested.store(true); }

bool is_shutdown_requested() { return shutdown_requested.load(); }