#include <atomic>
#include <cstdlib> // IWYU pragma: keep
#include <format>
#include <stdexcept>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>

#include <pulse/pulseaudio.h>

#include "audio.hxx"
#include "audio/pulseaudio_callbacks.hxx"

class PulseaudioBackend : public AudioBackend {
public:
  std::vector<AudioDevice> get_inputs() override;
  std::vector<AudioDevice> get_outputs() override;
  void record(AudioDevice dev) override;
  void stop_recording() override;

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
          "Pulseaudio connection failed with error {}. Reason: {}.",
          connection_err, pa_strerror(connection_err));
    }

    wait_for_context_ready();
  }

  ~PulseaudioBackend() {
    stop_recording();
    pa_context_disconnect(this->ctx);
    pa_context_unref(this->ctx);
    pa_mainloop_free(this->mainloop);
  }

private:
  pa_context *ctx;
  pa_mainloop *mainloop;
  pa_mainloop_api *api;
  pa_stream *stream = nullptr;
  std::shared_ptr<spdlog::logger> logger;
  std::atomic<bool> recording{false};

  // Block the mainloop until the context reaches READY or FAILED state.
  void wait_for_context_ready() {
    int pa_ready = 0;
    pa_context_set_state_callback(this->ctx, pa_state_cb, &pa_ready);

    while (true) {
      if (pa_ready == 0) {
        pa_mainloop_iterate(this->mainloop, 1, nullptr);
        continue;
      }

      if (pa_ready == 2) {
        pa_context_disconnect(this->ctx);
        pa_context_unref(this->ctx);
        pa_mainloop_free(this->mainloop);

        this->logger->critical("Could not connect to pulseaudio server.");
        throw std::runtime_error(
            "FATAL: Could not connect to pulseaudio server.");
      }

      break;
    }
  }

  // Run the mainloop until a `pa_operation` completes or is cancelled.
  void wait_for_operation(pa_operation *op) {
    while (pa_operation_get_state(op) == PA_OPERATION_RUNNING) {
      pa_mainloop_iterate(this->mainloop, 1, nullptr);
    }
    pa_operation_unref(op);
  }

  // Block the mainloop until the stream transitions to READY or FAILED.
  void wait_for_stream_ready(pa_stream *s) {
    pa_stream_state_t state;
    while ((state = pa_stream_get_state(s)) != PA_STREAM_READY) {
      if (state == PA_STREAM_FAILED || state == PA_STREAM_TERMINATED) {
        this->logger->error("Stream entered failed/terminated state.");
        throw std::runtime_error("PulseAudio stream failed to become ready.");
      }
      pa_mainloop_iterate(this->mainloop, 1, nullptr);
    }
  }

  // Disconnect and unreference the active stream, if any.
  void destroy_stream() {
    if (this->stream == nullptr)
      return;

    pa_stream_set_read_callback(this->stream, nullptr, nullptr);
    pa_stream_set_state_callback(this->stream, nullptr, nullptr);

    if (pa_stream_get_state(this->stream) == PA_STREAM_READY) {
      pa_stream_disconnect(this->stream);
    }

    pa_stream_unref(this->stream);
    this->stream = nullptr;
  }

  static void stream_state_cb(pa_stream *s, void *userdata) {
    auto *self = static_cast<PulseaudioBackend *>(userdata);
    auto state = pa_stream_get_state(s);

    switch (state) {
    case PA_STREAM_READY:
      self->logger->info("Recording stream is ready.");
      break;
    case PA_STREAM_FAILED:
      self->logger->error("Recording stream failed: {}.",
                          pa_strerror(pa_context_errno(self->ctx)));
      self->recording = false;
      break;
    case PA_STREAM_TERMINATED:
      self->logger->info("Recording stream terminated.");
      self->recording = false;
      break;
    default:
      break;
    }
  }

  static void stream_read_cb(pa_stream *s, size_t nbytes, void *userdata) {
    auto *self = static_cast<PulseaudioBackend *>(userdata);
    const void *data;
    size_t length;

    while (pa_stream_peek(s, &data, &length) >= 0) {
      if (length == 0)
        break;

      if (data == nullptr) {
        // Hole in the buffer — skip it.
        pa_stream_drop(s);
        continue;
      }

      if (self->data_callback) {
        self->data_callback(static_cast<const uint8_t *>(data), length);
      }

      pa_stream_drop(s);
    }
  }
};

std::vector<AudioDevice> PulseaudioBackend::get_inputs() {
  std::vector<AudioDevice> devices;
  pa_device_info_userdata_t userdata = {.devices = &devices,
                                        .logger = this->logger};

  pa_operation *pa_op =
      pa_context_get_source_info_list(this->ctx, pa_sourcelist_cb, &userdata);

  wait_for_operation(pa_op);

  return devices;
}

std::vector<AudioDevice> PulseaudioBackend::get_outputs() {
  std::vector<AudioDevice> devices;
  pa_device_info_userdata_t userdata = {.devices = &devices,
                                        .logger = this->logger};

  pa_operation *pa_op =
      pa_context_get_sink_info_list(this->ctx, pa_sinklist_cb, &userdata);

  wait_for_operation(pa_op);

  return devices;
}

AudioBackend &AudioBackend::instance() {
  static PulseaudioBackend backend;
  return backend;
}

void PulseaudioBackend::record(AudioDevice dev) {
  if (!dev.is_source) {
    throw std::invalid_argument(
        std::format("Device \'{}\' is not a source.", dev.description));
  }

  // Tear down any previous stream before starting a new one.
  destroy_stream();

  static constexpr pa_sample_spec samplespec = {
      .format = PA_SAMPLE_S16LE,
      .rate = 48000,
      .channels = 2,
  };

  this->stream =
      pa_stream_new(this->ctx, "audpipe_input", &samplespec, nullptr);

  if (this->stream == nullptr) {
    this->logger->error("Stream creation failed for device \'{}\'.",
                        dev.description);
    throw std::runtime_error(std::format(
        "Stream creation failed whilst trying to record from device \'{}\'",
        dev.description));
  }

  // Wire up callbacks so we get notified of state changes and incoming data.
  pa_stream_set_state_callback(this->stream, stream_state_cb, this);
  pa_stream_set_read_callback(this->stream, stream_read_cb, this);

  pa_stream_flags_t flags = static_cast<pa_stream_flags_t>(
      PA_STREAM_ADJUST_LATENCY | PA_STREAM_AUTO_TIMING_UPDATE);

  int status =
      pa_stream_connect_record(this->stream, dev.name.c_str(), nullptr, flags);

  if (status != 0) {
    this->logger->error("Could not record from device \'{}\'.",
                        dev.description);
    destroy_stream();
    throw std::runtime_error(
        std::format("Call to pa_stream_connect_record failed. Could not record "
                    "from device \'{}\'.",
                    dev.description));
  }

  // Wait until PulseAudio reports the stream as ready.
  wait_for_stream_ready(this->stream);
  this->logger->info("Recording from device \'{}\'.", dev.description);

  // Run the mainloop — data arrives via stream_read_cb.
  this->recording = true;
  while (this->recording) {
    pa_mainloop_iterate(this->mainloop, 1, nullptr);
  }

  destroy_stream();
  this->logger->info("Recording stopped.");
}

void PulseaudioBackend::stop_recording() { this->recording = false; }
