#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>

/* Convert centavos to a "PHP X.XX" string into buf (must be >= 20 bytes) */
void fmt_centavos(int centavos, char *buf, int bufsz);

#endif 