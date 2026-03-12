#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <uvgrtp/lib.hh>
#include <uvgrtp/util.hh>

#include <iostream>

#include "audio.hxx"

constexpr uint16_t REMOTE_PORT = 8890;
constexpr uint16_t LOCAL_PORT = 8891;

int main() {
  auto stderr_log = spdlog::stderr_color_mt("audpipe");

  uvgrtp::context ctx;
  uvgrtp::session *sess = ctx.create_session("127.0.0.1");

  if (sess == nullptr) {
    stderr_log->error("Failed to create uvgRTP session. Must be OOM.");
    return 1;
  }

  int flags = RCE_SEND_ONLY;
  uvgrtp::media_stream *opus_stream =
      sess->create_stream(LOCAL_PORT, REMOTE_PORT, RTP_FORMAT_OPUS, flags);

  if (opus_stream == nullptr) {
    stderr_log->error("Failed to create opus stream.");
    return 1;
  }

  // We want to get some audio input. For now, accept microphone.
  auto &audio = AudioBackend::instance();
  auto inputs = audio.get_inputs();

  for (auto input : inputs) {
    std::cout << input.name << std::endl;
  }
}