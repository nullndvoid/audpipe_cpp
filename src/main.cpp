#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "cli.hxx"

int main(int argc, char **argv) {
  auto stderr_logger = spdlog::stderr_color_mt("audpipe");
  stderr_logger->set_level(spdlog::level::info);
  return parse_cli(argc, argv);
}
