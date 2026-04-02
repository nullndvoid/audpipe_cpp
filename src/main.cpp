#include "audio.hxx"
#include "client.hxx"
#include "config.hxx"
#include "server.hxx"
#include "shutdown.hxx"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <thread>

inline void print_usage() {
  std::cerr << "Usage: audpipe (client | server)\n"
               "\nOr run with --help to see more options"
            << std::endl;
}

inline void print_help() {
  std::cerr
      << "audpipe (client | server)\n"
         "Flags:\n"
         "\t-h, --help\t\tDisplay a list of flags and options.\n"
         "\t-c, --config\t\tOverride the default config file.\n"
         "\t-d, --daemonise\t\tRun in the background. Currently a no-op.\n"
         "\t-v\t\t\tVerbose logging (enables debug logs).\n"
         "\nClient mode: creates a virtual input and recieves audio from "
         "remote server.\n"
         "Server mode: records from a given input device and forwards to a "
         "remote client."
      << std::endl;
}

void asio_signal_handler(const asio::error_code &error, int signal_number,
                         asio::io_context &io_context) {
  auto logger = spdlog::get("audpipe");
  logger->critical("Got signal {}", strsignal(signal_number));

  if (!error) {
    request_shutdown();
    io_context.stop();
  } else if (error != asio::error::operation_aborted) {
    logger->error("Signal error: {}", error.message());
  }
}

int main(int argc, char **argv) {
  asio::io_context io;
  asio::signal_set signals(io, SIGINT, SIGTERM);

  signals.async_wait(std::bind(asio_signal_handler, std::placeholders::_1,
                               std::placeholders::_2, std::ref(io)));
  std::thread signal_thread([&io]() { io.run(); });

  auto stderr_logger = spdlog::stderr_color_mt("audpipe");
  stderr_logger->set_level(spdlog::level::debug);

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
  } else if (strcmp(subcommand, "--help") == 0 ||
             strcmp(subcommand, "-h") == 0) {
    print_help();
    exit(0);
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
      Client client(local_pair, remote_pair, io, cfg.connection);

      while (!is_shutdown_requested() && client.is_healthy()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }

      client.request_shutdown();
    } else {
      auto &audio = AudioBackend::instance();
      auto device = Server::choose_device_interactive(audio.get_inputs());
      Server server(local_pair, remote_pair, device, io);

      server.run();
      server.stop();
    }

  } catch (const std::exception &e) {
    io.stop();
    if (signal_thread.joinable()) {
      signal_thread.join();
    }

    stderr_logger->error("Quitting with exception: {}", e.what());

    exit(1);
  }

  io.stop();
  if (signal_thread.joinable()) {
    signal_thread.join();
  }
}
