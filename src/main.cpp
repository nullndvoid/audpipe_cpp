#include "server.hxx"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

constexpr uint16_t REMOTE_PORT = 8890;
constexpr uint16_t LOCAL_PORT = 8891;

int main() {
  auto stderr_log = spdlog::stderr_color_mt("audpipe");
  stderr_log->set_level(spdlog::level::info);

  Server serv = Server(std::string("127.0.0.1"), LOCAL_PORT, REMOTE_PORT);
}