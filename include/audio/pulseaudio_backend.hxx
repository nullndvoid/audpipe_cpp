#include "audio.hxx"

#include <memory>

#include <spdlog/spdlog.h>

#include <pulse/pulseaudio.h>

class PulseaudioBackend : public AudioBackend {
public:
  std::vector<AudioDevice> get_inputs() override;
  std::vector<AudioDevice> get_outputs() override;
  void record(AudioDevice dev) override;
  void stop_recording() override;

private:
  pa_context *ctx;
  pa_mainloop *mainloop;
  pa_mainloop_api *api;
  pa_stream *stream = nullptr;
  std::shared_ptr<spdlog::logger> logger;
  std::atomic<bool> recording{false};
};