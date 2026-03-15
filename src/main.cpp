#include "client.hxx"
#include "server.hxx"

#include <cstdint>

#include <format>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <CLI11.hpp>

constexpr uint16_t DEFAULT_REMOTE_PORT = 8890;
constexpr uint16_t DEFAULT_LOCAL_PORT = 8891;
#define DEFAULT_LOCAL_IP "0.0.0.0"

int main(int argc, char **argv) {
  auto stderr_log = spdlog::stderr_color_mt("audpipe");
  stderr_log->set_level(spdlog::level::info);

  CLI::App app("A tool to forward inputs over RTP.", "audpipe");
  argv = app.ensure_utf8(argv);
  app.config_formatter(std::make_shared<CLI::ConfigTOML>());

  std::string local_ip;
  uint16_t local_port;
  uint16_t remote_port;
  bool print_config{false};

  auto home_dir = std::getenv("HOME");
  if (home_dir == nullptr) {
    stderr_log->warn(
        "Could not get $HOME, config file will not be automatically read from "
        "\'~/.config/audpipe/audpipe.toml\'.");

    app.set_config("-c,--config");
  } else {
    std::string config_path =
        std::format("{}/.config/audpipe/audpipe.toml", home_dir);
    app.set_config("-c,--config", config_path, "Read in config, TOML format.")
        ->transform(CLI::FileOnDefaultPath(config_path));
  }

  app.get_formatter()->column_width(40);
  app.get_formatter()->enable_option_type_names(false);
  // Ignore extra fields, these could later be parsed as TOML if required.
  app.allow_config_extras(CLI::config_extras_mode::ignore);

  auto *client = app.add_subcommand(
      "client", "Client (remote audio stream to virtual input)");
  auto *server =
      app.add_subcommand("server", "Server (forwards input to remote client)");
  client->configurable(true);
  server->configurable(true);

  auto add_common_opts = [&](CLI::App *sub) {
    sub->add_option("-i,--local-ip", local_ip,
                    "Local IP address to bind RTP to.")
        ->check(CLI::ValidIPV4)
        ->default_val(std::string(DEFAULT_LOCAL_IP));
    sub->add_option("-l,--local-port", local_port, "Local port to bind RDP to.")
        ->check(CLI::Range(1024, 65535))
        ->default_val(DEFAULT_LOCAL_PORT);
    sub->add_option("-p,--remote-port", remote_port, "Remote port for RDP.")
        ->check(CLI::Range(1024, 65535))
        ->default_val(DEFAULT_REMOTE_PORT);

    auto *print_config_flag =
        sub->add_flag("--print-config", print_config,
                      "Print effective config and exit (TOML).");
    print_config_flag->configurable(false);
  };

  add_common_opts(client);
  add_common_opts(server);

  app.require_subcommand(1);

  CLI11_PARSE(app, argc, argv);

  auto print_section_config = [&](CLI::App *sub, bool desc) {
    std::cout << "[" + sub->get_name() + "]\n"
              << sub->config_to_str(true, desc);
  };

  auto subs = app.get_subcommands();

  if (print_config) {
    for (auto sub : subs) {
      if (sub->parsed())
        print_section_config(sub, false);
    }

    return 0;
  }

  if (app.got_subcommand(server)) {
    Server(local_ip, local_port, remote_port);
  } else if (app.got_subcommand(client)) {
    Client(local_ip, local_port, remote_port);
  }
}