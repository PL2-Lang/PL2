#include "pl2.h"

#include <assert.h>
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*** ----------------- transmute any char to uchar ----------------- ***/

static unsigned char transmute_u8(char i8) {
  return *(unsigned char*)(&i8);
}

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

void pl2_printSlice(void *ptr, pl2_Slice slice) {
  FILE *fp = (FILE*)ptr;
  for (const char *it = slice.start;
       it != slice.end;
       ++it) {
    fputc(*it, fp);
  }
}

char *pl2_unsafeIntoCStr(pl2_Slice slice) {
  if (pl2_isNullSlice(slice)) {
    return NULL;
  }
  *(char*)slice.end = '\0';
  return (char*)slice.start;
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
  assert(errorCode != 0 && "zero indicates non-error");

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
  assert(errorCode != 0 && "zero indicates non-error");

  error->extraData = extraData;
  error->errorCode = errorCode;
  strcpy(error->reason, reason);
}

_Bool pl2_isError(pl2_Error *error) {
  return error->errorCode != 0;
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
  if (prev != NULL) {
    prev->next = ret;
  }
  ret->next = next;
  if (next != NULL) {
    next->prev = ret;
  }
  ret->extraData = extraData;
  for (uint32_t i = 0; i < partCount; i++) {
    ret->parts[i] = parts[i];
  }
  memset(ret->parts + partCount, 0, sizeof(pl2_CmdPart));
  return ret;
}

void pl2_initProgram(pl2_Program *program) {
  program->commands = NULL;
  program->extraData = NULL;
}

void pl2_clearProgram(pl2_Program *program) {
  pl2_Cmd *iter = program->commands;
  while (iter != NULL) {
    pl2_Cmd *next = iter->next;
    free(iter);
    iter = next;
  }
}

void pl2_debugPrintProgram(const pl2_Program *program) {
  fprintf(stderr, "program commands\n");
  pl2_Cmd *cmd = program->commands;
  while (cmd != NULL) {
    fprintf(stderr, "\t[");
    for (size_t i = 0; !PL2_EMPTY_PART(cmd->parts + i); i++) {
      pl2_CmdPart *part = cmd->parts + i;
      if (pl2_isNullSlice(part->prefix)) {
        fprintf(stderr, "`%s`, ", pl2_unsafeIntoCStr(part->body));
      } else {
        fprintf(stderr, "(`%s`, `%s`), ",
                pl2_unsafeIntoCStr(part->prefix),
                pl2_unsafeIntoCStr(part->body));
      }
    }
    fprintf(stderr, "\b\b]\n");
    cmd = cmd->next;
  }
  fprintf(stderr, "end program commands\n");
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
static void parseLine(ParseContext *ctx, pl2_Error *error);
static void parseQuesMark(ParseContext *ctx, pl2_Error *error);
static void parsePart(ParseContext *ctx, pl2_Error *error);
static pl2_Slice parseId(ParseContext *ctx, pl2_Error *error);
static pl2_Slice parseStr(ParseContext *ctx, pl2_Error *error);
static void checkBufferSize(ParseContext *ctx, pl2_Error *error);
static void finishLine(ParseContext *ctx, pl2_Error *error);
static void skipWhitespace(ParseContext *ctx);
static void skipComment(ParseContext *ctx);
static char curChar(ParseContext *ctx);
static char *curCharPos(ParseContext *ctx);
static void nextChar(ParseContext *ctx);
static _Bool isIdChar(char ch);
static _Bool isLineEnd(char ch);
static char *shrinkConv(char *start, char *end);

pl2_Program pl2_parse(char *source, pl2_Error *error) {
  ParseContext *context = createParseContext(source);

  while (curChar(context) != '\0') {
    parseLine(context, error);
    if (pl2_isError(error)) {
      break;
    }
  }

  pl2_Program ret = context->program;
  free(context);
  return ret;
}

static ParseContext *createParseContext(char *src) {
  ParseContext *ret = (ParseContext*)malloc(
    sizeof(ParseContext) + 512 * sizeof(pl2_CmdPart)
  );

  pl2_initProgram(&ret->program);
  ret->listTail = NULL;
  ret->src = src;
  ret->srcIdx = 0;
  ret->line = 1;
  ret->col = 0;
  ret->mode = PARSE_SINGLE_LINE;

  ret->partBufferSize = 512;
  ret->partUsage = 0;
  memset(ret->partBuffer, 0, 512 * sizeof(pl2_CmdPart));
  return ret;
}

static void parseLine(ParseContext *ctx, pl2_Error *error) {
  assert(ctx->col == 0);
  if (curChar(ctx) == '?') {
    parseQuesMark(ctx, error);
    if (pl2_isError(error)) {
      return;
    }
  }

  while (1) {
    skipWhitespace(ctx);
    if (curChar(ctx) == '\0' || curChar(ctx) == '\n') {
      if (ctx->mode == PARSE_SINGLE_LINE) {
        finishLine(ctx, error);
      }
      if (ctx->mode == PARSE_MULTI_LINE && curChar(ctx) == '\0') {
        pl2_fillError(error, PL2_ERR_UNCLOSED_BEGIN,
                      "unclosed `?begin` block", NULL);
      }
      if (curChar(ctx) == '\n') {
        nextChar(ctx);
      }
      return;
    } else if (curChar(ctx) == '#') {
      skipComment(ctx);
    } else {
      parsePart(ctx, error);
      if (pl2_isError(error)) {
        return;
      }
    }
  }
}

static void parseQuesMark(ParseContext *ctx, pl2_Error *error) {
  assert(curChar(ctx) == '?');
  nextChar(ctx);

  const char *start = curCharPos(ctx);
  while (isalnum(curChar(ctx))) {
    nextChar(ctx);
  }
  const char *end = curCharPos(ctx);
  pl2_Slice slice = pl2_slice(start, end);
  
  if (pl2_sliceCmpCStr(slice, "begin")) {
    ctx->mode = PARSE_MULTI_LINE;
  } else if (pl2_sliceCmpCStr(slice, "end")) {
    ctx->mode = PARSE_SINGLE_LINE;
    finishLine(ctx, error);
  } else {
    pl2_fillError(error, PL2_ERR_UNKNOWN_QUES,
                  "unknown question mark operator", NULL);
  }
}

static void parsePart(ParseContext *ctx, pl2_Error *error) {
  pl2_CmdPart part;
  if (curChar(ctx) == '"') {
    pl2_Slice body = parseStr(ctx, error);
    if (pl2_isError(error)) {
      return;
    }
    part = pl2_cmdPart(pl2_nullSlice(), body);
  } else {
    pl2_Slice maybePrefix = parseId(ctx, error);
    if (pl2_isError(error)) {
      return;
    }
    if (curChar(ctx) == '"') {
      pl2_Slice body = parseStr(ctx, error);
      if (pl2_isError(error)) {
        return;
      }
      part = pl2_cmdPart(maybePrefix, body);
    } else {
      part = pl2_cmdPart(pl2_nullSlice(), maybePrefix);
    }
  }

  checkBufferSize(ctx, error);
  if (pl2_isError(error)) {
    return;
  }

  ctx->partBuffer[ctx->partUsage++] = part;
}

static pl2_Slice parseId(ParseContext *ctx, pl2_Error *error) {
  (void)error;
  const char *start = curCharPos(ctx);
  while (isIdChar(curChar(ctx))) {
    nextChar(ctx);
  }
  const char *end = curCharPos(ctx);
  return pl2_slice(start, end);
}

static pl2_Slice parseStr(ParseContext *ctx, pl2_Error *error) {
  assert(curChar(ctx) == '"');
  nextChar(ctx);

  char *start = curCharPos(ctx);
  while (curChar(ctx) != '"' && !isLineEnd(curChar(ctx))) {
    if (curChar(ctx) == '\\') {
      nextChar(ctx);
      nextChar(ctx);
    } else {
      nextChar(ctx);
    }
  }
  char *end = curCharPos(ctx);
  end = shrinkConv(start, end);
  
  if (curChar(ctx) == '"') {
    nextChar(ctx);
  } else {
    pl2_fillError(error, PL2_ERR_UNCLOSED_STR,
                  "unclosed string literal", NULL);
    return pl2_nullSlice();
  }
  return pl2_slice(start, end);
}

static void checkBufferSize(ParseContext *ctx, pl2_Error *error) {
  if (ctx->partBufferSize <= ctx->partUsage + 1) {
    pl2_fillError(error, PL2_ERR_PARTBUF,
                  "command parts exceed internal parse buffer",
                  NULL);
  }
}

static void finishLine(ParseContext *ctx, pl2_Error *error) {
  if (ctx->partUsage) {
    pl2_fillError(error, PL2_ERR_EMPTY_CMD,
                  "empty commands are not allowed", NULL);
    return;
  }
  if (ctx->listTail == NULL) {
    assert(ctx->program.commands == NULL);
    ctx->program.commands = 
      ctx->listTail = pl2_cmd(ctx->partBuffer);
  } else {
    ctx->listTail = pl2_cmd4(ctx->listTail, NULL, NULL, ctx->partBuffer);
  }
  memset(ctx->partBuffer, 0,
         sizeof(pl2_CmdPart) * ctx->partBufferSize);
  ctx->partUsage = 0;
}

static void skipWhitespace(ParseContext *ctx) {
  while (1) {
    switch (curChar(ctx)) {
    case ' ': case '\t': case '\f': case '\v': case '\r':
      nextChar(ctx);
      break;
    default:
      return;
    }
  }
}

static void skipComment(ParseContext *ctx) {
  assert(curChar(ctx) == '#');
  nextChar(ctx);

  while (!isLineEnd(curChar(ctx))) {
    nextChar(ctx);
  }

  if (curChar(ctx) == '\n') {
    nextChar(ctx);
  }
}

static char curChar(ParseContext *ctx) {
  return ctx->src[ctx->srcIdx];
}

static char *curCharPos(ParseContext *ctx) {
  return ctx->src + ctx->srcIdx;
}

static void nextChar(ParseContext *ctx) {
  if (ctx->src[ctx->srcIdx] == '\0') {
    return;
  } else {
    if (ctx->src[ctx->srcIdx] == '\n') {
      ctx->line += 1;
      ctx->col = 0;
    } else {
      ctx->col += 1;
    }
    ctx->srcIdx += 1;
  }
}

static _Bool isIdChar(char ch) {
  unsigned char uch = transmute_u8(ch);
  if (uch >= 128) {
    return 1;
  } else if (isalnum(ch)) {
    return 1;
  } else {
    switch (ch) {
    case '!': case '$': case '%': case '^': case '&': case '*':
    case '(': case ')': case '-': case '+': case '_': case '=':
    case '[': case ']': case '{': case '}': case '|': case '\\':
    case ':': case ';': case '\'': case ',': case '<': case '>':
    case '/': case '?': case '~': case '@': case '.':
      return 1;
    default:
      return 0;
    }
  }
}

static _Bool isLineEnd(char ch) {
  return ch == '\0' || ch == '\n';
}

static char *shrinkConv(char *start, char *end) {
  char *iter1 = start, *iter2 = start;
  while (iter1 != end) {
    if (iter1[0] == '\\') {
      switch (iter1[1]) {
      case 'n': *iter2++ = '\n'; iter1 += 2; break;
      case 'r': *iter2++ = '\r'; iter1 += 2; break;
      case 'f': *iter2++ = '\f'; iter1 += 2; break;
      case 'v': *iter2++ = '\v'; iter1 += 2; break;
      case 't': *iter2++ = '\t'; iter1 += 2; break;
      case 'a': *iter2++ = '\a'; iter1 += 2; break;
      case '"': *iter2++ = '\"'; iter1 += 2; break;
      case '0': *iter2++ = '\0'; iter1 += 2; break;
      default:
        *iter2++ = *iter1++;
      }
    } else {
      *iter2++ = *iter1++;
    }
  }
  return iter2;
}

/*** -------------------- Semantic-var parsing  -------------------- ***/

typedef struct st_sem_ver {
  uint16_t major;
  uint16_t minor;
  uint16_t patch;
  char postfix[15];
  _Bool exact;
} SemVer;

static SemVer zeroVersion(void);
static SemVer parseSemVer(const char *src, pl2_Error *error);
static _Bool semverCompatible(SemVer ver1, SemVer ver2);
static int semverCmp(SemVer ver1, SemVer ver2);
static void semverToString(SemVer ver, char *buffer);

static const char *parseUint16(const char *src,
                               uint16_t *output,
                               pl2_Error *error);

static void parseSemVerPostfix(const char *src,
                               char *output,
                               pl2_Error *error);

static SemVer zeroVersion(void) {
  SemVer ret;
  ret.major = 0;
  ret.minor = 0;
  ret.patch = 0;
  memset(ret.postfix, 0, 15);
  ret.exact = 0;
  return ret;
}

static SemVer parseSemVer(const char *src, pl2_Error *error) {
  SemVer ret = zeroVersion();
  if (src[0] == '^') {
    ret.exact = 1;
    src++;
  }
  
  src = parseUint16(src, &ret.major, error);
  if (pl2_isError(error)) {
    pl2_fillError(error, PL2_ERR_SEMVER_PARSE,
                  "missing major version", NULL);
    goto done;
  } else if (src[0] == '\0') {
    goto done;
  } else if (src[0] == '-') {
    goto parse_postfix;
  } else if (src[0] != '.') {
    pl2_fillError(error, PL2_ERR_SEMVER_PARSE,
                  "expected `.`", NULL);
    goto done;
  }
  
  src++;
  src = parseUint16(src, &ret.minor, error);
  if (pl2_isError(error)) {
    pl2_fillError(error, PL2_ERR_SEMVER_PARSE,
                  "missing minor version", NULL);
    goto done;
  } else if (src[0] == '\0') {
    goto done;
  } else if (src[0] == '-') {
    goto parse_postfix;
  } else if (src[0] != '.') {
    pl2_fillError(error, PL2_ERR_SEMVER_PARSE,
                  "expected `.`", NULL);
    goto done;
  }
  
  src++;
  src = parseUint16(src, &ret.patch, error);
  if (pl2_isError(error)) {
    pl2_fillError(error, PL2_ERR_SEMVER_PARSE,
                  "missing patch version", NULL);
    goto done;
  } else if (src[0] == '\0') {
    goto done;
  } else if (src[0] != '-') {
    pl2_fillError(error, PL2_ERR_SEMVER_PARSE,
                  "unterminated semver, expected `-` or `\\0`", NULL);
    goto done;
  } 
  
parse_postfix:
  parseSemVerPostfix(src, ret.postfix, error);
  
done:
  return ret;
}

static const char *parseUint16(const char *src,
                               uint16_t *output,
                               pl2_Error *error) {
  if (!isdigit(src[0])) {
    pl2_fillError(error, PL2_ERR_SEMVER_PARSE,
                  "expected numberic version", NULL);
    return NULL;
  }
  *output = 0;
  while (isdigit(src[0])) {
    *output *= 10;
    *output += src[0] - '0';
    ++src;
  }
  return src;
}

static void parseSemVerPostfix(const char *src,
                               char *output,
                               pl2_Error *error) {
  assert(src[0] == '-');
  ++src;
  if (src[0] == '\0') {
    pl2_fillError(error, PL2_ERR_SEMVER_PARSE,
                  "empty semver postfix", NULL);
    return;
  }
  for (size_t i = 0; i < 15; i++) {
    if (!(*output++ = *src++)) {
      return;
    }
  }
  if (src[0] != '\0') {
    pl2_fillError(error, PL2_ERR_SEMVER_PARSE,
                  "semver postfix too long", NULL);
    return;
  }
}

static _Bool semverCompatible(SemVer require, SemVer given) {
  /* TODO */
}

static int semverCmp(SemVer ver1, SemVer ver2) {
  if (!strncmp(ver1.postfix, ver2.postfix, 16)) {
    return 2;
  }
  
  if (ver1.major < ver2.major) {
    return -1;
  } else if (ver1.major > ver2.major) {
    return 1;
  } else if (ver1.minor < ver2.minor) {
    return -1;
  } else if (ver1.minor > ver2.minor) {
    return 1;
  } else if (ver1.patch < ver2.patch) {
    return -1;
  } else if (ver1.patch > ver2.patch) {
    return 1;
  } else {
    return 0;
  }
}

static void semverToString(SemVer ver, char *buffer) {
  if (ver.postfix[0]) {
    sprintf(buffer, "%s%u.%u.%u-%s",
            ver.exact ? "^" : "",
            ver.major,
            ver.minor,
            ver.patch,
            ver.postfix);
  } else {
    sprintf(buffer, "%s%u.%u.%u",
            ver.exact ? "^" : "",
            ver.major,
            ver.minor,
            ver.patch);
  }
}

/*** ------------- Implementation of PL2 builtin lang  ------------- ***/

/*
pl2_Cmd *pl2_func(pl2_Program*, void**, pl2_Cmd*, pl2_Error*);
*/

typedef struct st_sinvoke_func_info {
  pl2_SInvokeFunc func;
  pl2_Slice ffiFuncName;
  SemVer version;
} SInvokeFuncInfo;

typedef struct st_pcall_func_info {
} PCallFuncInfo;

typedef struct st_pl2_lang_extra_data {
  pl2_Context context;
  
} pl2_LangExtraData;

static pl2_Cmd* pl2_langInit(pl2_Program *program,
                             void **extraData,
                             pl2_Cmd *command,
                             pl2_Error *error) {
  (void)program;
  (void)extraData;
  (void)command;
  (void)error;
  return command->next;
}

