/* Android comparison diagnostics implemented alongside the texture counters. */
#ifndef ANDROID_STATE_REFRESH_TEST_H
#define ANDROID_STATE_REFRESH_TEST_H
void android_state_test_refresh(unsigned long shader, unsigned long modes, BOOL immediate);
void android_state_test_stage(const char *stage);
void android_state_test_texture(unsigned long stage, unsigned long data, unsigned long format);
#endif
