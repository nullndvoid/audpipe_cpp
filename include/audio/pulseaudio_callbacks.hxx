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
  // For debugging purposes.
  std::string const &target_name;
  pa_mainloop *ml;
} pa_module_userdata_t;

typedef int *success_userdata_t;

void pa_sinklist_cb(pa_context *c, const pa_sink_info *l, int eol,
                    void *userdata);

void pa_sourcelist_cb(pa_context *c, const pa_source_info *l, int eol,
                      void *userdata);

void pa_state_cb(pa_context *c, void *userdata);

void pa_load_module_cb(pa_context *c, uint32_t idx, void *userdata);

// Run the mainloop until a `pa_operation` completes or is cancelled.
// Throws std::runtime_error if `op` is `nullptr`.
void wait_for_operation(pa_operation *op, pa_mainloop *ml);

// Generic callback for when we need to know if something succeeded or not.
void pa_ctx_success_cb(pa_context *c, int success, void *userdata);
#endif