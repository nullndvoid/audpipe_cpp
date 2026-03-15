#include <algorithm>
#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <format>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include <spdlog/spdlog.h>

#include <pulse/pulseaudio.h>

#include "audio.hxx"
#include "audio/pulseaudio_backend.hxx"
#include "audio/pulseaudio_callbacks.hxx"

namespace {
volatile std::sig_atomic_t shutdown_requested = 0;

void handle_shutdown_signal(int) { shutdown_requested = 1; }
} // namespace

PulseaudioBackend::PulseaudioBackend() {
  this->logger = spdlog::get("audpipe");

  std::signal(SIGINT, handle_shutdown_signal);
  std::signal(SIGTERM, handle_shutdown_signal);
  std::signal(SIGPIPE, SIG_IGN);

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
    if (shutdown_requested != 0) {
      this->logger->info("Shutdown signal received; stopping recording.");
      this->recording = false;
      break;
    }

    pa_mainloop_iterate(this->mainloop, 1, nullptr);
  }

  destroy_stream();
  this->logger->info("Recording stopped.");
}

void PulseaudioBackend::stop_recording() {
  this->recording = false;
  this->playback = false;
}

void PulseaudioBackend::create_virtual_input() {
  if (this->virtual_source_loaded) {
    this->logger->warn("`create_virtual_input` called twice. You should only "
                       "need one! Ignoring request.");

    return;
  }

  ::unlink(this->virtual_source_fifo_path.c_str());
  if (::mkfifo(this->virtual_source_fifo_path.c_str(), 0600) != 0) {
    throw std::runtime_error(std::format("Failed to create fifo '{}': {}",
                                         this->virtual_source_fifo_path,
                                         std::strerror(errno)));
  }

  std::string target_name = "module-pipe-source";
  std::string module_args = std::format(
      "source_name=audpipe_input file={} format=s16le rate=48000 "
      "channels=2 source_properties=device.description=audpipe_input",
      this->virtual_source_fifo_path);

  pa_module_userdata_t ud = {
      .logger = this->logger,
      .mod_idx = &this->virtual_source_mod_idx,
      .target_name = target_name,
      .ml = this->mainloop,
  };

  auto op = pa_context_load_module(this->ctx, target_name.c_str(),
                                   module_args.c_str(), pa_load_module_cb, &ud);

  try {
    wait_for_operation(op, this->mainloop);
  } catch (const std::exception &err) {
    this->logger->error("Could not create virtual input: {}", err.what());
    ::unlink(this->virtual_source_fifo_path.c_str());
    throw;
  }

  if (this->virtual_source_mod_idx == PA_INVALID_INDEX) {
    ::unlink(this->virtual_source_fifo_path.c_str());
    throw std::runtime_error("PulseAudio failed to load module-pipe-source.");
  }

  while (this->virtual_source_fd < 0) {
    this->virtual_source_fd = ::open(this->virtual_source_fifo_path.c_str(),
                                     O_WRONLY | O_NONBLOCK | O_CLOEXEC);

    if (this->virtual_source_fd >= 0) {
      break;
    }

    if (shutdown_requested != 0) {
      int rollback_success = -1;
      auto op_unload =
          pa_context_unload_module(this->ctx, this->virtual_source_mod_idx,
                                   pa_ctx_success_cb, &rollback_success);
      if (op_unload != nullptr) {
        wait_for_operation(op_unload, this->mainloop);
      }
      ::unlink(this->virtual_source_fifo_path.c_str());
      throw std::runtime_error(
          "Shutdown requested before virtual source was ready.");
    }

    if (errno != ENXIO && errno != EINTR) {
      int rollback_success = -1;
      auto op_unload =
          pa_context_unload_module(this->ctx, this->virtual_source_mod_idx,
                                   pa_ctx_success_cb, &rollback_success);
      if (op_unload != nullptr) {
        wait_for_operation(op_unload, this->mainloop);
      }
      ::unlink(this->virtual_source_fifo_path.c_str());
      throw std::runtime_error(
          std::format("Failed to open fifo '{}' for writing: {}",
                      this->virtual_source_fifo_path, std::strerror(errno)));
    }

    pa_mainloop_iterate(this->mainloop, 0, nullptr);
    ::usleep(2000);
  }

  this->logger->info(
      "Created virtual source `audpipe_input` via module-pipe-source.");
  this->virtual_source_loaded = true;

  this->playback = true;
  std::vector<uint8_t> write_buf(3840, 0);

  this->logger->info(
      "Virtual microphone `audpipe_input` running. Choose `audpipe_input` "
      "as your input device.");

  while (this->playback) {
    if (shutdown_requested != 0) {
      this->logger->info(
          "Shutdown signal received; stopping virtual microphone.");
      this->playback = false;
      break;
    }

    size_t produced = 0;
    if (this->write_callback) {
      produced = this->write_callback(write_buf.data(), write_buf.size());
      if (produced > write_buf.size()) {
        produced = write_buf.size();
      }
    }
    if (produced < write_buf.size()) {
      std::fill(write_buf.begin() + static_cast<std::ptrdiff_t>(produced),
                write_buf.end(), 0);
    }

    size_t offset = 0;
    while (offset < write_buf.size()) {
      ssize_t wrote =
          ::write(this->virtual_source_fd, write_buf.data() + offset,
                  write_buf.size() - offset);
      if (wrote < 0) {
        if (errno == EINTR) {
          continue;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          if (shutdown_requested != 0) {
            this->playback = false;
            break;
          }

          pa_mainloop_iterate(this->mainloop, 0, nullptr);
          ::usleep(2000);
          continue;
        }

        if (errno == EPIPE) {
          this->logger->warn(
              "pipe-source reader disconnected; retrying write loop.");
          this->playback = false;
          break;
        }

        this->logger->error("Failed writing to pipe-source fifo: {}",
                            std::strerror(errno));
        this->playback = false;
        break;
      }
      offset += static_cast<size_t>(wrote);
    }
  }

  if (this->virtual_source_fd >= 0) {
    ::close(this->virtual_source_fd);
    this->virtual_source_fd = -1;
  }
  this->logger->info("Virtual microphone stopped.");
}

void PulseaudioBackend::destroy_virtual_input() {
  if (this->virtual_source_fd >= 0) {
    ::close(this->virtual_source_fd);
    this->virtual_source_fd = -1;
  }

  if (!this->virtual_source_loaded ||
      this->virtual_source_mod_idx == PA_INVALID_INDEX) {
    ::unlink(this->virtual_source_fifo_path.c_str());
    return;
  }

  int success = -1;
  auto op = pa_context_unload_module(this->ctx, this->virtual_source_mod_idx,
                                     pa_ctx_success_cb, &success);

  wait_for_operation(op, this->mainloop);

  if (success == 0) {
    this->logger->warn("pa_context_unload_module got error code {}", success);
    ::unlink(this->virtual_source_fifo_path.c_str());
    return;
  }

  this->logger->info("Unloaded virtual source module (index {}).",
                     this->virtual_source_mod_idx);
  this->virtual_source_loaded = false;
  this->virtual_source_mod_idx = PA_INVALID_INDEX;
  ::unlink(this->virtual_source_fifo_path.c_str());
}