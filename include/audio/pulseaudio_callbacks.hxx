#ifndef __AUDPIPE_PULSE_CALLBACKS
#define __AUDPIPE_PULSE_CALLBACKS

#include <memory>
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

typedef struct pa_module_userdata {
  std::shared_ptr<spdlog::logger> logger;
  // So we can unload a module later.
  int *mod_idx;
  pa_mainloop *ml;
} pa_module_userdata_t;

void pa_sinklist_cb(pa_context *c, const pa_sink_info *l, int eol,
                    void *userdata);

void pa_sourcelist_cb(pa_context *c, const pa_source_info *l, int eol,
                      void *userdata);

void pa_state_cb(pa_context *c, void *userdata);

void pa_load_module_cb(pa_context *c, uint32_t idx, void *userdata);

void pa_get_module_info_cb(pa_context *c, const pa_module_info *i, int eol,
                           void *userdata);

// Run the mainloop until a `pa_operation` completes or is cancelled.
void wait_for_operation(pa_operation *op, pa_mainloop *ml);
#endif