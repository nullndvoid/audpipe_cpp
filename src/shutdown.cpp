#include "shutdown.hxx"

#include <csignal>

static volatile std::sig_atomic_t shutdown_requested = 0;

static void handle_shutdown_signal(int) { shutdown_requested = 1; }

void install_signal_handlers() {
  std::signal(SIGINT, handle_shutdown_signal);
  std::signal(SIGTERM, handle_shutdown_signal);
  std::signal(SIGPIPE, SIG_IGN);
}

void request_shutdown() { shutdown_requested = 1; }

bool is_shutdown_requested() { return shutdown_requested == 1; }