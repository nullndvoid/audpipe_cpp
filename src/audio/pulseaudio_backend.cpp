#include <atomic>
#include <format>
#include <stdexcept>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>

#include <pulse/pulseaudio.h>

#include "audio.hxx"
#include "audio/pulseaudio_backend.hxx"
#include "audio/pulseaudio_callbacks.hxx"

PulseaudioBackend::PulseaudioBackend() {
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

PulseaudioBackend::~PulseaudioBackend() {
  stop_recording();
  destroy_virtual_input();

  pa_context_disconnect(this->ctx);
  pa_context_unref(this->ctx);
  pa_mainloop_free(this->mainloop);
}

// Block the mainloop until the context reaches READY or FAILED state.
void PulseaudioBackend::wait_for_context_ready() {
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

// Block the mainloop until the stream transitions to READY or FAILED.
void PulseaudioBackend::wait_for_stream_ready(pa_stream *s) {
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
void PulseaudioBackend::destroy_stream() {
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

std::vector<AudioDevice> PulseaudioBackend::get_inputs() {
  std::vector<AudioDevice> devices;
  pa_device_info_userdata_t userdata = {.devices = &devices,
                                        .logger = this->logger};

  pa_operation *pa_op =
      pa_context_get_source_info_list(this->ctx, pa_sourcelist_cb, &userdata);

  wait_for_operation(pa_op, this->mainloop);

  return devices;
}

std::vector<AudioDevice> PulseaudioBackend::get_outputs() {
  std::vector<AudioDevice> devices;
  pa_device_info_userdata_t userdata = {.devices = &devices,
                                        .logger = this->logger};

  pa_operation *pa_op =
      pa_context_get_sink_info_list(this->ctx, pa_sinklist_cb, &userdata);

  wait_for_operation(pa_op, this->mainloop);

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

  const pa_sample_spec samplespec = {
      .format = PA_SAMPLE_S16LE,
      .rate = 48000,
      .channels = dev.channels,
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

  // Request ~20ms fragments (one Opus frame worth of S16LE stereo audio).
  // We expect around 3840 bytes for this. Since fragsize = 20ms * 48000 Hz * 2
  // channels * 2 bytes/sample = 3840 bytes.
  pa_buffer_attr bufattr = {};
  bufattr.maxlength = static_cast<uint32_t>(-1);
  bufattr.fragsize = pa_usec_to_bytes(20 * PA_USEC_PER_MSEC, &samplespec);

  pa_stream_flags_t flags = static_cast<pa_stream_flags_t>(
      PA_STREAM_ADJUST_LATENCY | PA_STREAM_AUTO_TIMING_UPDATE);

  int status =
      pa_stream_connect_record(this->stream, dev.name.c_str(), &bufattr, flags);

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

void PulseaudioBackend::create_virtual_input() {
  if (this->virtual_sink_loaded) {
    this->logger->warn("`create_virtual_input` called twice. You should only "
                       "need one! Ignoring request.");

    return;
  }

  pa_module_userdata_t ud = {
      .logger = this->logger,
      .mod_idx = &this->virtual_sink_mod_idx,
      .target_name = "module-null-sink",
      .ml = this->mainloop,
  };

  constexpr auto module_args =
      "sink_name=audpipe_out "
      "sink_properties=device.description=Audpipe_Output";

  auto op = pa_context_load_module(this->ctx, "module-null-sink", module_args,
                                   pa_load_module_cb, &ud);

  try {
    wait_for_operation(op, this->mainloop);
  } catch (const std::exception &err) {
    this->logger->error("Could not create virtual input: {}", err.what());
    throw;
  }

  if (this->virtual_sink_mod_idx == PA_INVALID_INDEX) {
    throw std::runtime_error("PulseAudio failed to load module-null-sink.");
  }

  this->logger->info("Created virtual sink `audpipe_out`; use source "
                     "`audpipe_out.monitor` as the virtual input.");
  this->virtual_sink_loaded = true;
}

void PulseaudioBackend::destroy_virtual_input() {
  if (!this->virtual_sink_loaded) {
    return;
  }

  int success = -1;
  auto op = pa_context_unload_module(this->ctx, this->virtual_sink_mod_idx,
                                     pa_ctx_success_cb, &success);

  wait_for_operation(op, this->mainloop);

  if (success == 0) {
    this->logger->warn("pa_context_unload_module got error code {}", success);
  }
}