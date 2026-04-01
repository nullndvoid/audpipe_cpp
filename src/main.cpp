#include "audio.hxx"
#include "client.hxx"
#include "config.hxx"
#include "server.hxx"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

inline void print_usage() {
  std::cerr << "Usage: audpipe (client | server)" << std::endl;
}

int main(int argc, char **argv) {
  auto stderr_logger = spdlog::stderr_color_mt("audpipe");
  stderr_logger->set_level(spdlog::level::info);

  auto die = [&] {
    print_usage();
    exit(1);
  };

  if (argc < 2)
    die();

  auto subcommand = argv[1];
  Mode mode = {};

  if (strcmp(subcommand, "client") == 0) {
    mode = Mode::CLIENT;
  } else if (strcmp(subcommand, "server") == 0) {
    mode = Mode::SERVER;
  } else {
    die();
  }

  try {
    auto cfg = Config("./audpipe.toml", mode);

    stderr_logger->info("Starting {} on {}:{}.", Config::mode_to_str(mode),
                        cfg.local_ip, cfg.local_port);
    stderr_logger->info("Remote: {}:{}.", cfg.remote_ip, cfg.remote_port);

    std::pair local_pair = {cfg.local_ip, cfg.local_port};
    std::pair remote_pair = {cfg.remote_ip, cfg.remote_port};

    if (mode == Mode::CLIENT) {
      Client(local_pair, remote_pair);
    } else {
      auto &audio = AudioBackend::instance();

      Server(local_pair, remote_pair,
             Server::choose_device_interactive(audio.get_inputs()));
    }

  } catch (...) {
    // Presume it was logged upstream and just quit.
    exit(1);
  }
}
