#include "pl2ext.h"

#include <assert.h>
#include <stdarg.h>
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

/*** ----------------------- Arguments Helper ---------------------- ***/

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
  int32_t lowerBound;
  int32_t upperBound;
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

static uint16_t elementListLen(nacl_ElementBase *list[]);
static uint16_t elementVAListLen(va_list ap);

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

nacl_ElementBase *nacl_bool(uint16_t id) {
  nacl_ElementBase *base
    = (nacl_ElementBase*)malloc(sizeof(nacl_ElementBase));
  if (base != NULL) {
    base->elementType = NACL_BOOL;
    base->elementId = id;
  }
  return base;
}

nacl_ElementBase *nacl_boundInt(uint16_t id,
                                int32_t lowerBound,
                                int32_t upperBound) {
  BoundInt *boundInt = (BoundInt*)malloc(sizeof(BoundInt));
  if (boundInt != NULL) {
    boundInt->elementType = NACL_BOUND_INT;
    boundInt->elementId = id;
    boundInt->lowerBound = lowerBound;
    boundInt->upperBound = upperBound;
  }
  return (nacl_ElementBase*)boundInt;
}

nacl_ElementBase *nacl_userChar(uint16_t id, char ch) {
  UserChar *userChar = (UserChar*)malloc(sizeof(UserChar));
  if (userChar != NULL) {
    userChar->elementType = NACL_USER_CHAR;
    userChar->elementId = id;
    userChar->userChar = ch;
  }
  return (nacl_ElementBase*)userChar;
}

nacl_ElementBase *nacl_userString(uint16_t id, const char *str) {
  UserStr *userStr = (UserStr*)malloc(sizeof(UserStr));
  if (userStr != NULL) {
    userStr->elementType = NACL_USER_STR;
    userStr->elementId = id;
    userStr->userStr = str;
  }
  return (nacl_ElementBase*)userStr;
}

nacl_ElementBase *nacl_userFunc(uint16_t id, nacl_UserFuncStub *stub) {
  UserFunc *userFunc = (UserFunc*)malloc(sizeof(UserFunc));
  if (userFunc != NULL) {
    userFunc->elementType = NACL_USER_FUNC;
    userFunc->elementId = id;
    userFunc->stub = stub;
  }
  return (nacl_ElementBase*)userFunc;
}

nacl_ElementBase *nacl_optional(uint16_t id, nacl_ElementBase *base) {
  Optional *optional = (Optional*)malloc(sizeof(Optional));
  if (optional != NULL) {
    optional->elementType = NACL_OPTIONAL;
    optional->elementId = id;
    optional->base = base;
  }
  return (nacl_ElementBase*)optional;
}

nacl_ElementBase *nacl_repeated(uint16_t id, nacl_ElementBase *base) {
  Repeated *repeated = (Repeated*)malloc(sizeof(Repeated));
  if (repeated != NULL) {
    repeated->elementType = NACL_REPEATED;
    repeated->elementId = id;
    repeated->base = base;
  }
  return (nacl_ElementBase*)repeated;
}

nacl_ElementBase *nacl_sum(uint16_t id, ...) {
  va_list va;
  va_start(va, id);
  
  va_list va1;
  va_copy(va1, va);
  uint16_t len = elementVAListLen(va1);

  Sum *sum = (Sum*)malloc(
    sizeof(Sum) + (len + 1) * sizeof(nacl_ElementBase*)
  );
  if (sum != NULL) {
    sum->elementType = NACL_SUM;
    sum->elementId = id;
    for (uint16_t i = 0; i < len; i++) {
      sum->subElements[i] = va_arg(va, nacl_ElementBase*);
    }
    sum->subElements[len] = NULL;
  }
  return (nacl_ElementBase*)sum;
}

nacl_ElementBase *nacl_product(uint16_t id, ...) {
  va_list va;
  va_start(va, id);

  va_list va1;
  va_copy(va1, va);
  uint16_t len = elementVAListLen(va1);
  
  Product *product = (Product*)malloc(
    sizeof(Product) * (len + 1) * sizeof(nacl_ElementBase*)
  );
  if (product != NULL) {
    product->elementType = NACL_PRODUCT;
    product->elementId = id;
    for (uint16_t i = 0; i < len; i++) {
      product->subElements[i] = va_arg(va, nacl_ElementBase*);
    }
    product->subElements[len] = NULL;
  }
  return (nacl_ElementBase*)product;
}

void nacl_free(nacl_ElementBase *tree) {
  switch (tree->elementType) {
  case NACL_OPTIONAL:
    {
      Optional *optional = (Optional*)tree;
      nacl_free(optional->base);
      free(optional);
      break;
    }
  case NACL_REPEATED:
    {
      Repeated *repeated = (Repeated*)tree;
      nacl_free(repeated->base);
      free(repeated);
      break;
    }
  case NACL_SUM:
    {
      Sum *sum = (Sum*)tree;
      for (uint16_t i = 0; sum->subElements[i] != NULL; i++) {
        nacl_free(sum->subElements[i]);
      }
      free(sum);
      break;
    }
  case NACL_PRODUCT:
    {
      Product *product = (Product*)tree;
      for (uint16_t i = 0; product->subElements[i] != NULL; i++) {
        nacl_free(product->subElements[i]);
      }
      free(product);
      break;
    }
  default:
    free(tree);
  }
}

static uint16_t elementListLen(nacl_ElementBase *list[]) {
  uint16_t len = 0;
  for (; len < UINT16_MAX - 1 && list[len] != NULL; len++);
  return len;
}

static uint16_t elementVAListLen(va_list ap) {
  uint16_t len = 0;
  for (; len < UINT16_MAX - 1; len++) {
    nacl_ElementBase *thisElem = va_arg(ap, nacl_ElementBase*);
    if (thisElem == NULL) {
      va_end(ap);
      break;
    }
  }
  return len;
}

