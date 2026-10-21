#ifndef ANDROID_GEOMETRY_DIAGNOSTICS_H
#define ANDROID_GEOMETRY_DIAGNOSTICS_H
#if defined(HALO_ANDROID) && defined(HALO_GLES)
#define HALO_GEOMETRY_DIAGNOSTICS 1
extern unsigned long geometry_test_frame, geometry_test_draw;
extern int geometry_test_sample;
void geometry_test_log(const char *format, ...) __attribute__((format(printf, 1, 2)));
void geometry_test_error(const char *where, int force);
void geometry_test_present(unsigned long frame);
void geometry_test_map(const char *name);
#endif
#endif
