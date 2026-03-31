#include <filesystem>
#include <fstream>

#include <iostream>
#include <spdlog/spdlog.h>

#include <toml++/toml.hpp>

#include "config.hxx"

std::string Config::file_to_str(const std::filesystem::path &path) {
  auto logger = spdlog::get("audpipe");

  errno = 0;
  std::ifstream config_file = std::ifstream(path);
  try {
    config_file.exceptions(std::ios::failbit | std::ios::badbit);
  } catch (const std::ios_base::failure &e) {
    if (errno == 0) {
      throw;
    }

    logger->error("Could not open file at path \"{}\" because: {}",
                  path.native(), e.what());

    throw std::system_error{
        errno, std::generic_category(),
        std::format("Opening file at path \"{}\" failed because: {}",
                    path.native(), e.what())};
  }

  if (!config_file.is_open()) {
    logger->error("Could not open config file at \"{}\" for unknown reasons.",
                  path.native());
  }

  std::vector<char> config_contents(
      (std::istreambuf_iterator<char>(config_file)),
      std::istreambuf_iterator<char>());

  return std::string(config_contents.data());
}

Config::Config(const std::string &cfg_path) {
  auto logger = spdlog::get("audpipe");
  std::filesystem::path path;

  try {
    path = std::filesystem::path(cfg_path);
  } catch (const std::exception &e) {
    logger->error("Invalid path input \"{}\" threw exception: {}", cfg_path,
                  e.what());
    throw e;
  }

  auto contents = Config::file_to_str(path);

  // Now parse as TOML.
  logger->info("Read configuration file from \"{}\".", path.native());
  std::cout << contents << std::endl;
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