#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>

/*
 * Miscellaneous helpers used across the project.
 */

/* Convert centavos to a "PHP X.XX" string into buf (must be >= 20 bytes) */
void fmt_centavos(int64_t centavos, char *buf, int bufsz);

// /* Return the current monotonic time in milliseconds */
// long long time_now_ms(void);

// /* Safe string copy — always null-terminates, returns bytes written */
// int safe_strncpy(char *dst, const char *src, int dstsz);

// /* Clamp an integer between lo and hi */
// int clamp(int val, int lo, int hi);

#endif /* UTILS_H */