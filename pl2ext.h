#ifndef PL2EXT_H
#define PL2EXT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*** ------------------------- Error helper ------------------------ ***/

const char *pl2ext_explain(int errCode);

/*** ----------------------- Basic reparsing ----------------------- ***/

_Bool pl2ext_parseInt(const char *src, int64_t *out);
_Bool pl2ext_parseDouble(const char *src, double *out);
const char *pl2ext_startsWith(const char *src, const char *prefix);


/*** ---------------------- Arguments helper ----------------------- ***/

uint16_t pl2ext_argLen(const char **args);
_Bool pl2ext_checkArgsLen(const char **args,
                          uint16_t minArgLen,
                          uint16_t maxArgLen);

/*** ----------------------------- NaCl ---------------------------- ***/

typedef struct st_nacl_slice {
  const char *start;
  const char *end;
} nacl_Slice;

nacl_Slice nacl_slice(const char *start, const char *end);

typedef enum e_nacl_element_type {
  NACL_INT,
  NACL_NUMBER,
  NACL_BOOL,

  NACL_BOUND_INT,
  NACL_USER_CHAR,
  NACL_USER_STR,
  NACL_USER_FUNC,

  NACL_OPTIONAL,
  NACL_REPEATED,
  NACL_SUM,
  NACL_PRODUCT
} nacl_ElementType;

typedef struct st_nacl_element_base {
  uint16_t elementType;
  uint16_t elementId;
} nacl_ElementBase;

const nacl_ElementBase NACL_SENTRY = {0, 0};

typedef const char* (nacl_UserFuncStub)(const char *src);

nacl_ElementBase *nacl_int(uint16_t id);
nacl_ElementBase *nacl_number(uint16_t id);
nacl_ElementBase *nacl_bool(uint16_t id);
nacl_ElementBase *nacl_boundInt(uint16_t id,
                                uint64_t lowerBound,
                                uint64_t upperBound);
nacl_ElementBase *nacl_userChar(uint16_t id, char ch);
nacl_ElementBase *nacl_userString(uint16_t id, const char *str);
nacl_ElementBase *nacl_userFunc(uint16_t id, nacl_UserFuncStub *stub);
nacl_ElementBase *nacl_optional(uint16_t id, nacl_ElementBase *base);
nacl_ElementBase *nacl_repeated(uint16_t id, nacl_ElementBase *base);
nacl_ElementBase *nacl_sum(uint16_t id,
                           nacl_ElementBase *subElements[]);
nacl_ElementBase *nacl_product(uint16_t id,
                               nacl_ElementBase *subElements[]);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // PL2EXT_H
