#include "pl2ext.h"

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/*** ------------------------- Error helper ------------------------ ***/

typedef enum {
  ERR_NONE           = 0,
  ERR_GENERAL        = 1,
  ERR_PARSEBUF       = 2,
  ERR_UNCLOSED_STR   = 3,
  ERR_UNCLOSED_BEGIN = 4,
  ERR_EMPTY_CMD      = 5,
  ERR_SEMVER_PARSE   = 6,
  ERR_UNKNOWN_QUES   = 7,
  ERR_LOAD_LANG      = 8,
  ERR_NO_LANG        = 9,
  ERR_UNKNOWN_CMD    = 10,
  ERR_MALLOC         = 11,

  ERR_USER           = 100
} ErrorCode;

static const char *errCodeMaps[] = {
    [ERR_NONE]           = "no error",
    [ERR_GENERAL]        = "general hard error",
    [ERR_PARSEBUF]       = "parsing buffer exceeded",
    [ERR_UNCLOSED_STR]   = "unclosed string",
    [ERR_UNCLOSED_BEGIN] = "unclosed `?begin` block",
    [ERR_EMPTY_CMD]      = "empty command",
    [ERR_SEMVER_PARSE]   = "semantic version parsing error",
    [ERR_UNKNOWN_QUES]   = "unknown `?` operator",
    [ERR_LOAD_LANG]      = "cannot load desired language",
    [ERR_NO_LANG]        = "no language loaded yet",
    [ERR_UNKNOWN_CMD]    = "unknown command",
    [ERR_MALLOC]         = "malloc failed"
};

const char *pl2ext_explain(int errCode) {
  if (errCode >= ERR_USER) {
    return "user defined error";
  } else if (errCode > ERR_UNKNOWN_CMD && errCode < ERR_USER) {
    return "unused error code";
  } else {
    return errCodeMaps[errCode];
  }
}

/*** ----------------------- Basic reparsing ----------------------- ***/

_Bool pl2ext_parseInt(const char *src, int64_t *out) {
  assert(src != NULL && out != NULL);
  (void)src;
  (void)out;
  return 0; // TODO not implemented
}
_Bool pl2ext_parseDouble(const char *src, double *out) {
  assert(src != NULL && out != NULL);
  (void)src;
  (void)out;
  return 0; // TODO not implemented
}

const char *pl2ext_startsWith(const char *src, const char *prefix) {
  assert(src != NULL && prefix != NULL);
  size_t srcLen = strlen(src);
  size_t prefixLen = strlen(prefix);
  assert(prefixLen != 0);
  if (srcLen < prefixLen) {
    return 0;
  }

  if (!strncmp(src, prefix, prefixLen)) {
    return src + prefixLen;
  } else {
    return NULL;
  }
}

/*** ----------------------------- NaCl ---------------------------- ***/

uint16_t pl2ext_argLen(const char **args) {
  assert(args != NULL);
  uint16_t acc = 0;
  for (; *args != NULL; acc++, args++);
  return acc;
}

_Bool pl2ext_checkArgsLen(const char **args,
                          uint16_t minArgLen,
                          uint16_t maxArgLen) {
  uint16_t len = pl2ext_argLen(args);
  return len >= minArgLen && len <= maxArgLen;
}

/*** ----------------------------- NaCl ---------------------------- ***/

#define ELEMENT_COMMON \
  uint16_t elementType;   \
  uint16_t elementId;

typedef struct st_bound_int {
  ELEMENT_COMMON
  int64_t lowerBound;
  int64_t upperBound;
} BoundInt;

typedef struct st_user_char {
  ELEMENT_COMMON
  char userChar;
} UserChar;

typedef struct st_user_str {
  ELEMENT_COMMON
  const char *userStr;
} UserStr;

typedef struct st_user_func {
  ELEMENT_COMMON
  nacl_UserFuncStub *stub;
} UserFunc;

typedef struct st_optional {
  ELEMENT_COMMON
  nacl_ElementBase *base;
} Optional;

typedef struct st_repeated {
  ELEMENT_COMMON
  nacl_ElementBase *base;
} Repeated;

typedef struct st_sum {
  ELEMENT_COMMON
  nacl_ElementBase *subElements[0];
} Sum;

typedef struct st_product {
  ELEMENT_COMMON
  nacl_ElementBase *subElements[0];
} Product;

nacl_ElementBase *nacl_int(uint16_t id) {
  nacl_ElementBase *base
    = (nacl_ElementBase*)malloc(sizeof(nacl_ElementBase));
  if (base != NULL) {
    base->elementType = NACL_INT;
    base->elementId = id;
  }
  return base;
}

nacl_ElementBase *nacl_number(uint16_t id) {
  nacl_ElementBase *base
    = (nacl_ElementBase*)malloc(sizeof(nacl_ElementBase));
  if (base != NULL) {
    base->elementType = NACL_NUMBER;
    base->elementId = id;
  }
  return base;
}
