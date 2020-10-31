#include "pl2.h"

#include <assert.h>
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*** ----------------- Implementation of pl2_Slice ----------------- ***/

pl2_Slice pl2_slice(const char *start, const char *end) {
  pl2_Slice ret;
  if (start == end) {
    ret.start = NULL;
    ret.end = NULL;
  } else {
    ret.start = start;
    ret.end = end;
  }
  return ret;
}

pl2_Slice pl2_nullSlice(void) {
  pl2_Slice ret;
  ret.start = NULL;
  ret.end = NULL;
  return ret;
}

pl2_Slice pl2_sliceFromCStr(const char *cStr) {
  const char *cStrEnd = cStr;
  while (*cStrEnd++ != '\0');
  return pl2_slice(cStr, cStrEnd);
}

_Bool pl2_sliceCmp(pl2_Slice s1, pl2_Slice s2) {
  if (s1.start == s2.start && s1.end == s2.end) {
    return 1;
  } else {
    size_t s1Len = pl2_sliceLen(s1);
    size_t s2Len = pl2_sliceLen(s2);
    if (s1Len == s2Len) {
      return !strncmp(s1.start, s2.start, s1Len);
    } else {
      return 0;
    }
  }
}

_Bool pl2_sliceCmpCStr(pl2_Slice slice, const char *cStr) {
  for (const char *it = slice.start; it != slice.end; it++, cStr++) {
    if (*it != *cStr) {
      return 0;
    }
  }
  if (*cStr != '\0') {
    return 0;
  }
  return 1;
}

size_t pl2_sliceLen(pl2_Slice slice) {
  assert(slice.end >= slice.start);
  return slice.end - slice.start;
}

_Bool pl2_isNullSlice(pl2_Slice slice) {
  return slice.start == slice.end;
}

/*** ---------------- Implementation of pl2_MString ---------------- ***/

static uint32_t BKDRHash(const char *str) {
  uint32_t hash = 0;
  char ch = 0;
  ch = *str++;
  while (ch) {
    hash = hash * 13 + ch;
    ch = *str++;
  }
  return hash;
}

typedef struct st_hash_map_item {
  struct st_hash_map_item *next;
  char value[0];
} HashMapItem;

static HashMapItem *newHashMapItem(const char *value) {
  HashMapItem *ret = (HashMapItem*)malloc(sizeof(HashMapItem)
                                          + strlen(value)
                                          + 1);
  ret->next = NULL;
  strcpy(ret->value, value);
  return ret;
}

#define HASH_BUCKETS 4096
typedef struct st_mcontext_impl {
  HashMapItem *buckets[HASH_BUCKETS];
} MContextImpl;

pl2_MContext *pl2_mContext(void) {
  MContextImpl *impl = (MContextImpl*)malloc(sizeof(MContextImpl));
  memset(impl, 0, sizeof(MContextImpl));
  return (pl2_MContext*)impl;
}

void pl2_mContextFree(pl2_MContext *context) {
  MContextImpl *impl = (MContextImpl*)context;
  for (uint32_t i = 0; i < HASH_BUCKETS; i++) {
    HashMapItem *item = impl->buckets[i];
    while (item != NULL) {
      HashMapItem *nextItem = item->next;
      free(item);
      item = nextItem;
    }
  }
  free(context);
}

pl2_MString pl2_mString(pl2_MContext *context, const char *string) {
  MContextImpl *impl = (MContextImpl*)context;
  
  uint32_t hash = BKDRHash(string);
  uint32_t bucket = hash % 4096;
  if (impl->buckets[bucket] == NULL) {
    impl->buckets[bucket] = newHashMapItem(string);
    return bucket * 40960 + 1;
  } else if (strcmp(string, impl->buckets[bucket]->value) == 0) {
    return bucket * 40960 + 1;
  } else {
    HashMapItem *prevItem = impl->buckets[bucket];
    HashMapItem *curItem = prevItem->next;

    uint32_t idx = 2;
    while (curItem != NULL) {
      if (strcmp(string, curItem->value) == 0) {
        return bucket * 40960 + idx;
      }
      prevItem = curItem;
      curItem = curItem->next;
      idx += 1;
    }

    prevItem->next = newHashMapItem(string);
    return bucket * 40960 + idx;
  }
}

const char *pl2_getString(pl2_MContext *context, pl2_MString mstring) {
  MContextImpl *impl = (MContextImpl*)context;
  
  uint32_t bucket = mstring / 40960;
  uint32_t idx = mstring % 40960;
  
  uint32_t i = 1;
  HashMapItem *item = impl->buckets[bucket];
  
  while (i < idx) {
    i += 1;
    item = item->next;
    assert(item != NULL);
  }

  return item->value;
}

/*** ----------------- Implementation of pl2_Error ----------------- ***/

pl2_Error *pl2_error(uint16_t errorCode,
                     const char *reason,
                     void *extraData) {
  size_t len = strlen(reason);
  pl2_Error *ret = (pl2_Error*)malloc(sizeof(pl2_Error) + len + 1);
  ret->extraData = extraData;
  ret->errorCode = errorCode;
  strcpy(ret->reason, reason);
  return ret;
}

pl2_Error *pl2_errorBuffer(size_t strBufferSize) {
  pl2_Error *ret = (pl2_Error*)malloc(sizeof(pl2_Error) + strBufferSize);
  memset(ret, 0, sizeof(pl2_Error) + strBufferSize);
  return ret;
}

void pl2_fillError(pl2_Error *error,
                   uint16_t errorCode,
                   const char *reason,
                   void *extraData) {
  error->extraData = extraData;
  error->errorCode = errorCode;
  strcpy(error->reason, reason);
}

/*** ------------------- Some toolkit functions -------------------- ***/

pl2_SourceInfo pl2_sourceInfo(const char *fileName, uint16_t line) {
  pl2_SourceInfo ret;
  ret.fileName = fileName;
  ret.line = line;
  return ret;
}

pl2_CmdPart pl2_cmdPart(pl2_Slice prefix, pl2_Slice body) {
  pl2_CmdPart ret;
  ret.prefix = prefix;
  ret.body = body;
  return ret;
}

pl2_Cmd *pl2_cmd(pl2_CmdPart *parts) {
  return pl2_cmd4(NULL, NULL, NULL, parts);
}

pl2_Cmd *pl2_cmd4(pl2_Cmd *prev,
                  pl2_Cmd *next,
                  void *extraData,
                  pl2_CmdPart *parts) {
  uint32_t partCount = 0;
  for (pl2_CmdPart *part = parts;
       !PL2_EMPTY_PART(part);
       part++, partCount++);
  pl2_Cmd *ret = (pl2_Cmd*)malloc(sizeof(pl2_Cmd) +
                                  (partCount+1) * sizeof(pl2_CmdPart));
  ret->prev = prev;
  ret->next = next;
  ret->extraData = extraData;
  for (uint32_t i = 0; i < partCount; i++) {
    ret->parts[i] = parts[i];
  }
  memset(ret->parts + partCount, 0, sizeof(pl2_CmdPart));
  return ret;
}

void pl2_initProgram(pl2_Program *program) {
  program->language = pl2_nullSlice();
  program->commands = NULL;
  program->extraData = NULL;
}

/*** ----------------- Implementation of pl2_parse ----------------- ***/

typedef enum e_parse_mode {
  PARSE_SINGLE_LINE = 1,
  PARSE_MULTI_LINE  = 2
} ParseMode;

typedef enum e_ques_cmd {
  QUES_INVALID = 0,
  QUES_BEGIN = 1,
  QUES_END   = 2
} QuesCmd;

typedef struct st_parse_context {
  pl2_Program program;
  pl2_Cmd *listTail;
  
  char *src;
  uint32_t srcIdx;
  uint16_t line, col;
  ParseMode mode;
  
  uint32_t partBufferSize;
  uint32_t partUsage;
  pl2_CmdPart partBuffer[0];
} ParseContext;

static ParseContext *createParseContext(char *src);

pl2_Program pl2_parse(char *source, pl2_Error *error);

static ParseContext *createParseContext(char *src) {
  ParseContext *ret = (ParseContext*)malloc(
    sizeof(ParseContext) + 512 * sizeof(pl2_CmdPart)
  );

  pl2_initProgram(&ret->program);

  ret->listTail = NULL;
  ret->src = src;
  ret->srcIdx = 0;
  ret->line = 0;
  ret->col = 0;
  ret->mode = PARSE_SINGLE_LINE;
  
  ret->partBufferSize = 512;
  ret->partUsage = 0;
  
  return ret;
}









