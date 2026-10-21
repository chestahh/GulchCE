/* Local diagnostic capture for the Android geometry investigation APK. */
#ifndef GEOMETRY_DIAGNOSTICS_H
#define GEOMETRY_DIAGNOSTICS_H
void host_geometry_begin(const char *root);
void host_geometry_log(const char *line);
void host_geometry_event(const char *format, ...) __attribute__((format(printf, 1, 2)));
#endif
