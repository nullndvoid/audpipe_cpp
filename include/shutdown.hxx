#ifndef __AUDPIPE_SHUTDOWN
#define __AUDPIPE_SHUTDOWN

void install_signal_handlers();
bool is_shutdown_requested();
void request_shutdown();

#endif