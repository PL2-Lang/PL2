#include "pl2.h"

#include <assert.h>
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

pl2_MContext *pl2_mContext() {
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

/*** ------------------- Some toolkit functions -------------------- ***/

pl2_SourceInfo pl2_sourceInfo(const char *fileName, uint16_t line) {
  pl2_SourceInfo ret;
  ret.fileName = fileName;
  ret.line = line;
  return ret;
}

pl2_CmdPart pl2_cmdPart(const char *prefix, const char *body) {
  pl2_CmdPart ret;
  ret.prefix = prefix;
  ret.body = body;
  return ret;
}
start:
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

/*** ----------------- Implementation of pl2_parse ----------------- ***/

typedef struct st_parse_context {
  pl2_Program program;
  pl2_Cmd *listTail;
  
  char *src;
  uint32_t srcIdx;
  uint16_t line, col;
  
  uint32_t partBufferSize;
  uint32_t partUsage;
  pl2_CmdPart partBuffer[0];
} ParseContext;

static ParseContext *createParseContext(char *source);

pl2_Program pl2_parse(char *source) {
  pl2_Program ret;
  ret->language = NULL;
  ret->libName = NULL;
  ret->commands = NULL;
  ret->extraData = NULL;
  
  pl2_Cmd *listTail = NULL;
  pl2_Cmd *thisCmd = NULL;
  static pl2_CmdPart partBuf[512];
  uint32_t thisPart = 0;
  uint32_t srcIdx = 0;
  uint16_t line = 1, col = 0;
}






