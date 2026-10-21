/* The native window boundary uses plain C types, independent of the XDK ABI. */
#ifndef HALO_VULKAN_WINDOW_H
#define HALO_VULKAN_WINDOW_H
int halo_vulkan_window_open(unsigned width, unsigned height, int hidden, int validation, const char *capture_path);
int halo_vulkan_window_close(void);
void halo_vulkan_window_clear(const float color[4]);
int halo_vulkan_window_present(void);
unsigned halo_vulkan_window_submitted(void);
unsigned halo_vulkan_window_generations(void);
/* Pumps events, returns false on close; no game input is consumed in test mode. */
int halo_vulkan_window_events(void);
void halo_vulkan_window_resize(unsigned width, unsigned height);
void halo_vulkan_window_suspend(int suspend);
unsigned long long halo_vulkan_window_ticks(void);
void halo_vulkan_window_delay(void);
#endif
