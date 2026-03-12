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
