#include <cstdlib> // IWYU pragma: keep
#include <pulse/context.h>
#include <pulse/def.h>
#include <pulse/error.h>
#include <pulse/mainloop-api.h>
#include <vector>

#include <spdlog/spdlog.h>

#include <pulse/introspect.h>
#include <pulse/mainloop.h>
#include <pulse/pulseaudio.h>

#include "audio.hxx"

class PulseaudioBackend : public AudioBackend {
public:
  std::vector<AudioDevice> get_inputs() override;
  void open(const AudioDevice &device) override;
  void close() override;

  PulseaudioBackend() {
    auto stderr_log = spdlog::get("audpipe");
    setenv("PULSE_PROP_application.name", "audpipe", 1);
    setenv("PULSE_PROP_application.icon_name", "audpipe", 1);
    setenv("PULSE_PROP_media.role", "phone", 1);

    this->mainloop = pa_mainloop_new();
    this->api = pa_mainloop_get_api(mainloop);
    this->ctx = pa_context_new(api, "audpipe");

    auto connection_err =
        pa_context_connect(ctx, nullptr, PA_CONTEXT_NOFLAGS, nullptr);

    if (connection_err < 0) {
      stderr_log->error(
          "Pulseaudio connection failed with error {d}. Reason: {}.",
          connection_err, pa_strerror(connection_err));
    }
  }

  ~PulseaudioBackend() {
    pa_context_disconnect(this->ctx);
    pa_context_unref(this->ctx);
    pa_mainloop_free(this->mainloop);
  }

private:
  pa_context *ctx;
  pa_mainloop *mainloop;
  pa_mainloop_api *api;
};

std::vector<AudioDevice> PulseaudioBackend::get_inputs() {

  pa_source_info_cb_t cb;
  auto op = pa_context_get_source_info_list(this->ctx, cb, nullptr);

  std::vector<AudioDevice> devices;

  return devices;
}
