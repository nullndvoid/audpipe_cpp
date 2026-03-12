#include <cstdlib> // IWYU pragma: keep
#include <memory>
#include <pulse/def.h>
#include <pulse/proplist.h>
#include <pulse/sample.h>
#include <pulse/stream.h>
#include <stdexcept>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>

#include <pulse/pulseaudio.h>

#include "audio.hxx"

typedef struct pa_device_info {
  std::string name;
} pa_device_info_t;

typedef struct pa_device_info_userdata {
  std::vector<AudioDevice> *devices;
  std::shared_ptr<spdlog::logger> logger;
} pa_device_info_userdata_t;

void pa_state_cb(pa_context *c, void *userdata);
void pa_sourcelist_cb(pa_context *c, const pa_source_info *l, int eol,
                      void *userdata);

class PulseaudioBackend : public AudioBackend {
public:
  std::vector<AudioDevice> get_inputs() override;
  std::vector<AudioDevice> get_outputs() override;

  // void open(const AudioDevice &device) override;
  // void close() override;

  PulseaudioBackend() {
    this->logger = spdlog::get("audpipe");

    setenv("PULSE_PROP_application.name", "audpipe", 1);
    setenv("PULSE_PROP_application.icon_name", "audpipe", 1);
    setenv("PULSE_PROP_media.role", "phone", 1);

    this->mainloop = pa_mainloop_new();
    this->api = pa_mainloop_get_api(mainloop);
    this->ctx = pa_context_new(api, "audpipe");

    auto connection_err =
        pa_context_connect(ctx, nullptr, PA_CONTEXT_NOFLAGS, nullptr);

    if (connection_err < 0) {
      this->logger->error(
          "Pulseaudio connection failed with error {d}. Reason: {}.",
          connection_err, pa_strerror(connection_err));
    }

    // // Wait until connection is ready.
    int pa_ready = 0;
    pa_context_set_state_callback(this->ctx, pa_state_cb, &pa_ready);

    while (true) {
      if (pa_ready == 0) {
        pa_mainloop_iterate(this->mainloop, 1, nullptr);
        continue;
      }

      if (pa_ready == 2) {
        // Could not connect to the server. Quit.
        cleanup_pulseaudio_data(this->ctx, this->mainloop);

        this->logger->critical("Could not connect to pulseaudio server.");

        throw std::runtime_error(
            "FATAL: Could not connect to pulseaudio server.");
      }

      // If ready, then break.
      break;
    }
  }

  ~PulseaudioBackend() { cleanup_pulseaudio_data(this->ctx, this->mainloop); }

private:
  pa_context *ctx;
  pa_mainloop *mainloop;
  pa_mainloop_api *api;
  std::shared_ptr<spdlog::logger> logger;

  void record(AudioDevice dev);

  static void cleanup_pulseaudio_data(pa_context *ctx, pa_mainloop *ml) {
    pa_context_disconnect(ctx);
    pa_context_unref(ctx);
    pa_mainloop_free(ml);
  }
};

// Many of the `cb` boilerplate callbacks are sourced and adapted from
// the wonderful Andrew Kelley.
//
// https://gist.github.com/andrewrk/6470f3786d05999fcb48
// TODO: Add debug logging to userdata?
void pa_state_cb(pa_context *c, void *userdata) {
  int *pa_ready = static_cast<int *>(userdata);
  auto state = pa_context_get_state(c);

  switch (state) {
  // Just here for reference.
  case PA_CONTEXT_UNCONNECTED:
  case PA_CONTEXT_CONNECTING:
  case PA_CONTEXT_AUTHORIZING:
  case PA_CONTEXT_SETTING_NAME:
  default:
    break;
  case PA_CONTEXT_FAILED:
  case PA_CONTEXT_TERMINATED:
    *pa_ready = 2;
    break;
  case PA_CONTEXT_READY:
    *pa_ready = 1;
    break;
  }
}

void pa_sourcelist_cb(pa_context *c, const pa_source_info *l, int eol,
                      void *userdata) {

  pa_device_info_userdata &data =
      *reinterpret_cast<pa_device_info_userdata *>(userdata);

  // Reached end of list.
  if (eol > 0) {
    return;
  }

  bool is_monitor = l->monitor_of_sink != PA_INVALID_INDEX;
  auto device =
      AudioDevice(std::string(l->name), std::string(l->description), l->index,
                  l->sample_spec.rate, l->sample_spec.channels, is_monitor);

  data.logger->info("Found source device #{} \'{}\' with sample rate {}.",
                    device.index, device.description, device.sample_rate);
  data.devices->push_back(device);
}

void pa_sinklist_cb(pa_context *c, const pa_sink_info *l, int eol,
                    void *userdata) {

  pa_device_info_userdata &data =
      *reinterpret_cast<pa_device_info_userdata *>(userdata);

  // Reached end of list.
  if (eol > 0) {
    return;
  }

  auto device =
      AudioDevice(std::string(l->name), std::string(l->description), l->index,
                  l->sample_spec.rate, l->sample_spec.channels, false);

  data.logger->info("Found sink device #{} \'{}\' with sample rate {}.",
                    device.index, device.description, device.sample_rate);
  data.devices->push_back(device);
}

std::vector<AudioDevice> PulseaudioBackend::get_inputs() {
  std::vector<AudioDevice> devices;
  pa_device_info_userdata_t userdata = {.devices = &devices,
                                        .logger = this->logger};

  pa_operation *pa_op =
      pa_context_get_source_info_list(this->ctx, pa_sourcelist_cb, &userdata);

  while (pa_operation_get_state(pa_op) == PA_OPERATION_RUNNING) {
    pa_mainloop_iterate(this->mainloop, 1, nullptr);
  }

  pa_operation_unref(pa_op);

  return devices;
}

std::vector<AudioDevice> PulseaudioBackend::get_outputs() {
  std::vector<AudioDevice> devices;
  pa_device_info_userdata_t userdata = {.devices = &devices,
                                        .logger = this->logger};

  pa_operation *pa_op =
      pa_context_get_sink_info_list(this->ctx, pa_sinklist_cb, &userdata);

  while (pa_operation_get_state(pa_op) == PA_OPERATION_RUNNING) {
    pa_mainloop_iterate(this->mainloop, 1, nullptr);
  }

  pa_operation_unref(pa_op);

  return devices;
}

AudioBackend &AudioBackend::instance() {
  static PulseaudioBackend backend;
  return backend;
}

void PulseaudioBackend::record(AudioDevice dev) {
  // TODO: Check this later.
  static constexpr pa_sample_spec samplespec = {
      .format = PA_SAMPLE_S16LE,
      .rate = 48000,
      .channels = 2,
  };

  pa_proplist *p = pa_proplist_new();

  pa_stream *stream = pa_stream_new_with_proplist(this->ctx, "audpipe_input",
                                                  &samplespec, nullptr, p);
  if (stream == nullptr) {
    // Something went wrong. TODO: What?
  }

  pa_stream_flags_t flags = static_cast<pa_stream_flags_t>(0);
  int status =
      pa_stream_connect_record(stream, dev.name.c_str(), nullptr, flags);

  if (status != 0) {
    // Something went wrong.
  }

  pa_proplist_free(p);
}