#include "pl2.h"

#include <assert.h>
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
  HashMapItem buckets[HASH_BUCKETS];
} MContextImpl;

pl2_MContext *pl2_mContext() {
  MContextImpl *impl = (MContextImpl*)malloc(sizeof(MContextImpl));
  memset(impl, 0, sizeof(MContextImpl));
  return (pl2_MContext*)impl;
}

void pl2_mContextFree(pl2_MContext *context) {
  MContextImpl *impl = (MContextImpl*)context;
  for (size_t i = 0; i < HASH_BUCKETS; i++) {
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

    prevItem->next = nweHashmapItem(string);
    return bucket * 40960 + idx;
  }
}

const char *pl2_getString(pl2_MContext *context, pl2_MString mstring) {
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
