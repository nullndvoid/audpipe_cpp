#include <cstdint>
#include <filesystem>
#include <fstream>

#include <iostream>
#include <spdlog/spdlog.h>

#include <toml++/toml.hpp>

#include "config.hxx"

// Returns the opposing mode to ensure users provide the remote connection
// details.
Mode remote(Mode m) {
  return (m == Mode::CLIENT) ? Mode::SERVER : Mode::CLIENT;
}

// Converts the `Mode` to a string, lowercase.
std::string mode_to_str(Mode m) {
  return (m == Mode::CLIENT) ? "client" : "server";
}

std::string Config::file_to_str(const std::filesystem::path &path) {
  auto logger = spdlog::get("audpipe");
  auto p = std::filesystem::canonical(path);

  errno = 0;
  std::ifstream config_file = std::ifstream(p);
  try {
    config_file.exceptions(std::ios::failbit | std::ios::badbit);
  } catch (const std::ios_base::failure &e) {
    if (errno == 0) {
      throw;
    }

    logger->error("Could not open file at path \'{}\' because: {}", p.native(),
                  e.what());

    throw std::system_error{
        errno, std::generic_category(),
        std::format("Opening file at path \'{}\' failed because: {}",
                    p.native(), e.what())};
  }

  if (!config_file.is_open()) {
    logger->error("Could not open config file at \'{}\' for unknown reasons.",
                  p.native());
  }

  std::vector<char> config_contents(
      (std::istreambuf_iterator<char>(config_file)),
      std::istreambuf_iterator<char>());

  return std::string(config_contents.data());
}

Config::Config(const std::string &cfg_path, Mode mode) {
  auto logger = spdlog::get("audpipe");
  std::filesystem::path path;

  try {
    path = std::filesystem::path(cfg_path);
  } catch (const std::exception &e) {
    logger->error("Invalid path input \'{}\' threw exception: {}", cfg_path,
                  e.what());
    throw e;
  }

  auto p = std::filesystem::canonical(path);
  auto contents = Config::file_to_str(p);

  // Now parse as TOML.
  toml::table table;
  try {
    table = toml::parse(contents);
  } catch (const toml::parse_error &err) {
    logger->error("Failed to parse TOML config from file \'{}\'", p.native());
    logger->error("Reason: {} (line {}, column {})", err.what(),
                  err.source().begin.line, err.source().begin.column);

    throw;
  }

  logger->info("Successfully read configuration file from \'{}\'.",
               path.native());

  // Must be present or we shit the bed.
  auto remote_table_name = mode_to_str(remote(mode));
  auto remotes = table[remote_table_name];

  // May be null, in which case we stick with defaults.
  auto local_table_name = mode_to_str(mode);
  auto locals = table[local_table_name];

  if (!remotes || !remotes.is_table()) {
    auto err_msg =
        std::format("Expected table `[{}]` in config file.", remote_table_name);

    logger->error(err_msg);

    throw std::runtime_error(err_msg);
  }

  // TODO: Validate IPs, ports after reading config.
  auto ip = remotes["ip"].value<std::string>();
  if (!ip.has_value()) {
    auto err_msg =
        std::format("You need to set {}.ip in config file.", remote_table_name);

    logger->error(err_msg);

    throw std::runtime_error(err_msg);
  }

  this->remote_ip = ip.value();

  auto port = locals["port"].value_exact<uint16_t>();
  if (!port.has_value()) {
    auto err_msg = std::format("You need to set {}.port in config file. The "
                               "port should also be in the range 1024-65535.",
                               remote_table_name);

    logger->error(err_msg);

    throw std::runtime_error(err_msg);
  }
  this->remote_port = port.value();

  // Then, check `mode` table for ip and port. If not present, defaults are
  // already set, and we will log these values later.
  if (!locals) {
    return;
  }

  this->local_ip = locals["ip"].value_or<std::string>(DEFAULT_LOCAL_IP);

  auto local_port = locals["port"].value_exact<uint16_t>();
  if (!local_port.has_value()) {
    auto err_msg = std::format("You need to set {}.port in config file. The "
                               "port should also be in the range 1024-65535.",
                               local_table_name);

    logger->error(err_msg);

    throw std::runtime_error(err_msg);
  }

  this->local_port = local_port.value();
}

std::optional<std::string> Config::get_user_config_path() {
  auto logger = spdlog::get("audpipe");
  auto home_dir = std::getenv("HOME");

  if (home_dir == nullptr) {
    logger->error("Could not get $HOME, config file will not be automatically "
                  "read from "
                  "\'~/.config/audpipe/audpipe.toml\'.");
  }

  return std::nullopt;
}