/* Focused extraction harnesses without a real MCC runtime test conversion
 * behavior; diagnostic formatting has its own real-runtime regression tests. */
#include "../port/linux/game/mcc_diagnostics.h"
void mcc_diagnostic_tag(struct mcc_runtime *r, uint32_t handle) {(void)r;(void)handle;}
void mcc_diagnostic_at(struct mcc_runtime *r, char const *format, ...) {(void)r;(void)format;}
int mcc_diagnostic_fail(struct mcc_runtime *r, char const *format, ...) {(void)r;(void)format;return 0;}
