#include <algorithm>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <uvgrtp/lib.hh>
#include <uvgrtp/util.hh>

#include "audio.hxx"
#include "rtp.hxx"

constexpr uint16_t REMOTE_PORT = 8890;
constexpr uint16_t LOCAL_PORT = 8891;

int main() {
  auto stderr_log = spdlog::stderr_color_mt("audpipe");

  auto local_address = std::string("127.0.0.1");
  auto rtp = Rtp(local_address, LOCAL_PORT, REMOTE_PORT, RTP_SEND);

  auto &audio = AudioBackend::instance();
  auto inputs = audio.get_inputs();

  audio.set_data_callback([&](const uint8_t *data, size_t len) {
    // TODO: Encode to Opus and send via RTP.
    stderr_log->debug("Received {} bytes of audio data.", len);
  });

  // Find first monitor input.
  auto monitor = std::ranges::find_if(
      inputs, [&](AudioDevice &dev) { return dev.is_monitor; });

  stderr_log->info("First monitor device found is \'{}\'.",
                   monitor->description);

  audio.record(*monitor.base());
}