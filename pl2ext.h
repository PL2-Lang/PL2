#ifndef PL2EXT_H
#define PL2EXT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

_Bool pl2_parseInt(const char *src, int64_t *out);
_Bool pl2_parseDouble(const char *src, double *out);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // PL2EXT_H
