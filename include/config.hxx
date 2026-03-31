// Defines we might want to set application-wide. Currently most of these are
// just consumed in `parse_cli`.
#ifndef __AUDPIPE_CONFIG
#define __AUDPIPE_CONFIG

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

#define DEFAULT_REMOTE_PORT 8890
#define DEFAULT_LOCAL_PORT 8891
#define DEFAULT_LOCAL_IP "127.0.0.1"

class Config {
public:
  // Default behaviour should be to pass cwd/audpipe.toml. If this is empty we
  // can find ~/.config/audpipe/audpipe.toml.
  //
  // Should make some public static helper methods to get the paths as needed.
  Config(const std::string &cfg_path);

  static std::optional<std::string> get_user_config_path();

private:
  std::string config_path;
  std::string local_ip = DEFAULT_LOCAL_IP;
  std::string remote_ip = DEFAULT_LOCAL_IP;
  uint16_t remote_port = DEFAULT_REMOTE_PORT;
  uint16_t local_port = DEFAULT_LOCAL_PORT;

  static std::string file_to_str(const std::filesystem::path &path);
};

#endif