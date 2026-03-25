#include "audio.hxx"
#include "client.hxx"
#include "config.hxx"
#include "server.hxx"

#include <cstdint>
#include <filesystem>
#include <format>

#include <spdlog/spdlog.h>

#include <CLI11.hpp>

int parse_cli(int argc, char **argv) {
  auto logger = spdlog::get("audpipe");

  CLI::App app("A tool to forward inputs over RTP.", "audpipe");
  argv = app.ensure_utf8(argv);
  app.config_formatter(std::make_shared<CLI::ConfigTOML>());

  std::string local_ip;
  std::string remote_ip;

  uint16_t local_port;
  uint16_t remote_port;
  bool print_config{false};
  bool has_default_config_file{false};

  auto *print_config_flag =
      app.add_flag("--print-config", print_config,
                   "Print effective config and exit (TOML).");
  print_config_flag->configurable(false);

  auto home_dir = std::getenv("HOME");
  if (home_dir == nullptr) {
    logger->warn(
        "Could not get $HOME, config file will not be automatically read from "
        "\'~/.config/audpipe/audpipe.toml\'.");

    app.set_config("-c,--config");
  } else {
    std::string config_path =
        std::format("{}/.config/audpipe/audpipe.toml", home_dir);
    has_default_config_file = std::filesystem::exists(config_path);
    if (has_default_config_file) {
      app.set_config("-c,--config", config_path, "Read in config, TOML format.")
          ->transform(CLI::FileOnDefaultPath(config_path));
    } else {
      logger->info("Config file is missing, consider re-running with "
                   "--print-config\nand creating a file in "
                   "~/.config/audpipe/audpipe.toml");
      app.set_config("-c,--config", "", "Read in config, TOML format.");
    }
  }

  app.get_formatter()->column_width(40);
  app.get_formatter()->enable_option_type_names(false);
  // Ignore extra fields, these could later be parsed as TOML if required.
  app.allow_config_extras(CLI::config_extras_mode::ignore);

  bool cli_selected_client{false};
  bool cli_selected_server{false};
  bool cli_requested_print_config{false};
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "client")
      cli_selected_client = true;
    else if (arg == "server")
      cli_selected_server = true;
    else if (arg == "--print-config")
      cli_requested_print_config = true;
  }

  auto *client = app.add_subcommand(
      "client", "Client (remote audio stream to virtual input)");
  auto *server =
      app.add_subcommand("server", "Server (forwards input to remote client)");

  client
      ->add_option("-l,--local-port", local_port, "Local port to bind RDP to.")
      ->check(CLI::Range(1024, 65535))
      ->default_val(DEFAULT_LOCAL_PORT);
  client->add_option("-p,--remote-port", remote_port, "Remote port for RDP.")
      ->check(CLI::Range(1024, 65535))
      ->default_val(DEFAULT_REMOTE_PORT);

  server
      ->add_option("-l,--local-port", local_port, "Local port to bind RDP to.")
      ->check(CLI::Range(1024, 65535))
      ->default_val(DEFAULT_REMOTE_PORT);
  server->add_option("-p,--remote-port", remote_port, "Remote port for RDP.")
      ->check(CLI::Range(1024, 65535))
      ->default_val(DEFAULT_LOCAL_PORT);

  auto allow_client_config =
      cli_selected_client ||
      (!cli_selected_server && cli_requested_print_config);
  auto allow_server_config =
      cli_selected_server ||
      (!cli_selected_client && cli_requested_print_config);
  client->configurable(allow_client_config);
  server->configurable(allow_server_config);

  auto add_common_opts = [&](CLI::App *sub) {
    sub->add_option("-i,--local-ip", local_ip,
                    "Local IP address to bind RTP to.")
        ->check(CLI::ValidIPV4)
        ->default_val(std::string(DEFAULT_LOCAL_IP));
    sub->add_option("-r,--remote-ip", remote_ip,
                    "The IP the remote client or server is running on.")
        ->check(CLI::ValidIPV4)
        ->required(!cli_requested_print_config);

    auto *sub_print_config_flag =
        sub->add_flag("--print-config", print_config,
                      "Print effective config and exit (TOML).");
    sub_print_config_flag->configurable(false);
  };

  add_common_opts(client);
  add_common_opts(server);

  app.require_subcommand(0, 1);

  CLI11_PARSE(app, argc, argv);

  auto print_section_config = [&](CLI::App *sub, bool desc) {
    std::cout << "[" + sub->get_name() + "]\n"
              << sub->config_to_str(true, desc);
  };

  if (print_config) {
    if (auto cfg = std::dynamic_pointer_cast<CLI::ConfigBase>(
            app.get_config_formatter())) {
      cfg->commentDefaults(has_default_config_file);
    }

    if (cli_selected_server) {
      print_section_config(server, false);
    } else if (cli_selected_client) {
      print_section_config(client, false);
    } else {
      print_section_config(client, false);
      std::cout << "\n";
      print_section_config(server, false);
    }

    return 0;
  }

  if (!cli_selected_server && !cli_selected_client) {
    std::cout << app.help();
    return 1;
  }

  auto local_socket = std::pair(local_ip, local_port);
  auto remote_socket = std::pair(remote_ip, remote_port);

  if (cli_selected_server) {
    // TODO: Get input device from CLI or config.
    auto &audio = AudioBackend::instance();
    auto inputs = audio.get_inputs();
    auto input = Server::choose_device_interactive(inputs);

    logger->info("Selected device \'{}\'.", input.description);

    Server(local_socket, remote_socket, input);
  } else if (cli_selected_client) {
    Client(local_socket, remote_socket);
  }

  return 0;
}