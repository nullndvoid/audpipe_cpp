#include <pulse/pulseaudio.h>

#include "audio/pulseaudio_backend.hxx"
#include "audio/pulseaudio_callbacks.hxx"

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
  auto device = AudioDevice(std::string(l->name), std::string(l->description),
                            l->index, l->sample_spec.rate,
                            l->sample_spec.channels, is_monitor, true);

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
                  l->sample_spec.rate, l->sample_spec.channels, false, false);

  data.logger->info("Found sink device #{} \'{}\' with sample rate {}.",
                    device.index, device.description, device.sample_rate);
  data.devices->push_back(device);
}

void PulseaudioBackend::stream_state_cb(pa_stream *s, void *userdata) {
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

void PulseaudioBackend::stream_read_cb(pa_stream *s, size_t nbytes,
                                       void *userdata) {
  auto *self = static_cast<PulseaudioBackend *>(userdata);
  const void *data;
  size_t length;

  while (pa_stream_peek(s, &data, &length) >= 0) {

    if (length == 0)
      break;

    if (data == nullptr) {
      // Hole in the buffer, skip it.
      pa_stream_drop(s);
      continue;
    }

    if (self->data_callback) {
      self->data_callback(static_cast<const uint8_t *>(data), length);
    }

    pa_stream_drop(s);
  }
}
