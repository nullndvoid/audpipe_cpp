#include "shutdown.hxx"

#include <atomic>

namespace {
std::atomic<bool> shutdown_requested = false;
}

void request_shutdown() { shutdown_requested.store(true); }

bool is_shutdown_requested() { return shutdown_requested.load(); }