#include "pl2a.h"
#include "pl2ext.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

/*** -------------------------- bb_MiniVec ------------------------- ***/

typedef struct st_bb_mini_vec {
  void **ptr;
  uint16_t size;
  uint16_t cap;
} bb_MiniVec;

static void bb_initVec(bb_MiniVec *miniVec);
static void bb_dropVec(bb_MiniVec *miniVec);

static void bb_vecPushBack(bb_MiniVec *vec, void *elem);
static uint16_t bb_vecSize(bb_MiniVec *vec);
static void* bb_vecGet(bb_MiniVec *vec, uint16_t index);

static void bb_initVec(bb_MiniVec *miniVec) {
  memset(miniVec, 0, sizeof(miniVec));
}

static void bb_dropVec(bb_MiniVec *miniVec) {
  free(miniVec->ptr);
}

static void bb_vecPushBack(bb_MiniVec *vec, void *elem) {
  if (vec->size == vec->cap) {
    if (vec->ptr == NULL) {
      vec->ptr = (void**)malloc(16 * sizeof(void*));
      vec->size = 0;
      vec->cap = 16;
    } else {
      void **new_ptr = (void**)malloc(2 * vec->cap * sizeof(void*));
      memcpy(new_ptr, vec->ptr, vec->cap * sizeof(void*));
      free(vec->ptr);
      vec->ptr = new_ptr;
      vec->cap *= 2;
    }
  }
  vec->ptr[vec->size] = elem;
  vec->size += 1;
}

static uint16_t bb_vecSize(bb_MiniVec *vec) {
  return vec->size;
}

static void* bb_vecGet(bb_MiniVec *vec, uint16_t index) {
  assert(index < vec->size);
  return vec->ptr[index];
}

/*** --------------------------- bb_Value -------------------------- ***/

typedef enum st_bb_value_tag {
  BB_NUMBER,
  BB_STRING
} bb_ValueTag;

typedef union st_bb_value_data {
  double numValue;
  char *strValue;
} bb_ValueData;

typedef struct st_bb_value {
  bb_ValueTag tag;
  bb_ValueData *data;
} bb_Value;

/*** --------------------------- bb_Scope -------------------------- ***/

typedef struct st_bb_scope_entry {
  const char *name;
  bb_Value value;
} bb_ScopeEntry;

typedef struct st_bb_scope {
  bb_MiniVec entries;
} bb_Scope;

extern pl2a_Language*
pl2ext_loadLanguage(pl2a_SemVer version, pl2a_Error *error);

static void* bb_init(pl2a_Error *error);
static void bb_atExit(void *context);

static pl2a_Cmd*
bb_fallback(pl2a_Program *program,
            void *context,
            pl2a_Cmd *cmd,
            pl2a_Error *error);

pl2a_Language *pl2ext_loadLanguage(pl2a_SemVer version,
                                   pl2a_Error *error) {
  (void)version;
  (void)error;

  static pl2a_Cmd termCmd;

  static pl2a_Language ret = {
    /*langName    = */ "BeBasc",
    /*langInfo    = */ "BASIC dialect Advocated by BE",
    /*termCmd     = */ &termCmd,

    /*init        = */ bb_init,
    /*atExit      = */ bb_atExit,
    /*sinvokeCmds = */ NULL,
    /*pCallCmds   = */ NULL,
    /*fallback    = */ bb_fallback
  };

  return &ret;
}
