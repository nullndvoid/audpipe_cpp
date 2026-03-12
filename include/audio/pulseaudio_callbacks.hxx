
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

void pa_sinklist_cb(pa_context *c, const pa_sink_info *l, int eol,
                    void *userdata);

void pa_sourcelist_cb(pa_context *c, const pa_source_info *l, int eol,
                      void *userdata);

void pa_state_cb(pa_context *c, void *userdata);
