#include "utils.h"
#include <stdio.h>
#include <time.h>


void fmt_centavos(int64_t centavos, char *buf, int bufsz)
{
    int negative = (centavos < 0);
    if (negative) centavos = -centavos;
    snprintf(buf, (size_t)bufsz, "%sPHP %ld.%02d",
             negative ? "-" : "",
             centavos / 100,
             centavos % 100);
}