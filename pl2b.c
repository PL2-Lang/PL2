#include "pl2b.h"

#include <assert.h>
#include <ctype.h>
#include <dlfcn.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/*** ----------------- Implementation of versioning ---------------- ***/

const char *pl2b_getLocaleName(void) {
  static char *lang = NULL;
  static const char *ret = NULL;
  if (lang == NULL) {
    lang = getenv("LANG");
    if (lang == NULL) {
      lang = "en";
    }
    if (!strncmp(lang, "zh", 2)) {
      ret = PL2_EDITION_CN;
    } else if (!strncmp(lang, "ru", 2)) {
      ret = PL2_EDITION_RU;
    } else if (!strncmp(lang, "ko", 2)) {
      ret =  PL2_EDITION_KR;
    } else if (!strncmp(lang, "ja", 2)) {
      ret = PL2_EDITION_JP;
    } else {
      ret = NULL;
    }
  }

  return ret;
}

/*** ----------------- Transmute any char to uchar ----------------- ***/

static unsigned char transmuteU8(char i8) {
  return *(unsigned char*)(&i8);
}

/*** ------------------- Implementation of slice ------------------- ***/

typedef struct st_slice {
  char *start;
  char *end;
} Slice;

static Slice slice(char *start, char *end);
static Slice nullSlice(void);
static char *sliceIntoCStr(Slice slice);
static _Bool isNullSlice(Slice slice);

Slice slice(char *start, char *end) {
  Slice ret;
  if (start == end) {
    ret.start = NULL;
    ret.end = NULL;
  } else {
    ret.start = start;
    ret.end = end;
  }
  return ret;
}

static Slice nullSlice(void) {
  Slice ret;
  ret.start = NULL;
  ret.end = NULL;
  return ret;
}

static char *sliceIntoCStr(Slice slice) {
  if (isNullSlice(slice)) {
    return NULL;
  }
  if (*slice.end != '\0') {
    *slice.end = '\0';
  }
  return slice.start;
}

static _Bool isNullSlice(Slice slice) {
  return slice.start == slice.end;
}

/*** ----------------- Implementation of pl2b_Error ---------------- ***/

pl2b_Error *pl2b_errorBuffer(uint16_t strBufferSize) {
  pl2b_Error *ret = (pl2b_Error*)malloc(sizeof(pl2b_Error) + strBufferSize);
  if (ret == NULL) {
    return NULL;
  }
  memset(ret, 0, sizeof(pl2b_Error) + strBufferSize);
  ret->errorBufferSize = strBufferSize;
  return ret;
}

void pl2b_errPrintf(pl2b_Error *error,
                    uint16_t errorCode,
                    pl2b_SourceInfo sourceInfo,
                    void *extraData,
                    const char *fmt,
                    ...) {
  error->errorCode = errorCode;
  error->extraData = extraData;
  error->sourceInfo = sourceInfo;
  if (error->errorBufferSize == 0) {
    return;
  }

  va_list ap;
  va_start(ap, fmt);
  vsnprintf(error->reason, error->errorBufferSize, fmt, ap);
  va_end(ap);
}

void pl2b_dropError(pl2b_Error *error) {
  if (error->extraData) {
    free(error->extraData);
  }
  free(error);
}

_Bool pl2b_isError(pl2b_Error *error) {
  return error->errorCode != 0;
}

/*** ------------------- Some toolkit functions -------------------- ***/

pl2b_SourceInfo pl2b_sourceInfo(const char *fileName, uint16_t line) {
  pl2b_SourceInfo ret;
  ret.fileName = fileName;
  ret.line = line;
  return ret;
}

pl2b_CmdPart pl2b_cmdPart(char *str, _Bool isString) {
  return (pl2b_CmdPart) { str, isString };
}

pl2b_Cmd *pl2b_cmd3(pl2b_SourceInfo sourceInfo,
                    pl2b_CmdPart cmd,
                    pl2b_CmdPart args[]) {
  return pl2b_cmd6(NULL, NULL, NULL, sourceInfo, cmd, args);
}

pl2b_Cmd *pl2b_cmd6(pl2b_Cmd *prev,
                    pl2b_Cmd *next,
                    void *extraData,
                    pl2b_SourceInfo sourceInfo,
                    pl2b_CmdPart cmd,
                    pl2b_CmdPart args[]) {
  uint16_t argLen = 0;
  for (; !PL2B_EMPTY_PART(args[argLen]); ++argLen);

  pl2b_Cmd *ret = (pl2b_Cmd*)malloc(sizeof(pl2b_Cmd) +
                                    (argLen + 1) * sizeof(pl2b_CmdPart));
  if (ret == NULL) {
    return NULL;
  }
  ret->prev = prev;
  if (prev != NULL) {
    prev->next = ret;
  }
  ret->next = next;
  if (next != NULL) {
    next->prev = ret;
  }
  ret->sourceInfo = sourceInfo;
  ret->cmd = cmd;
  ret->resolveCache = NULL;
  ret->extraData = extraData;
  for (uint16_t i = 0; i < argLen; i++) {
    ret->args[i] = args[i];
  }
  ret->args[argLen] = pl2b_cmdPart(NULL, 0);
  return ret;
}

uint16_t pl2b_argsLen(pl2b_Cmd *cmd) {
  uint16_t acc = 0;
  pl2b_CmdPart *iter = cmd->args;
  while (!PL2B_EMPTY_PART(*iter)) {
    iter++;
    acc++;
  }
  return acc;
}

/*** ---------------- Implementation of pl2b_Program --------------- ***/

void pl2b_initProgram(pl2b_Program *program) {
  program->commands = NULL;
}

void pl2b_dropProgram(pl2b_Program *program) {
  pl2b_Cmd *iter = program->commands;
  while (iter != NULL) {
    pl2b_Cmd *next = iter->next;
    free(iter);
    iter = next;
  }
}

void pl2b_debugPrintProgram(const pl2b_Program *program) {
  fprintf(stderr, "program commands\n");
  pl2b_Cmd *cmd = program->commands;
  while (cmd != NULL) {
    if (cmd->cmd.isString) {
      fprintf(stderr, "\t\"%s\" [", cmd->cmd.str);
    } else {
      fprintf(stderr, "\t%s [", cmd->cmd.str);
    }
    for (uint16_t i = 0; !PL2B_EMPTY_PART(cmd->args[i]); i++) {
      pl2b_CmdPart arg = cmd->args[i];
      if (arg.isString) {
        fprintf(stderr, "\"%s\", ", arg.str);
      } else {
        fprintf(stderr, "`%s`, ", arg.str);
      }
    }
    fprintf(stderr, "\b\b]\n");
    cmd = cmd->next;
  }
  fprintf(stderr, "end program commands\n");
}

/*** ----------------- Implementation of pl2b_parse ---------------- ***/

typedef enum e_parse_mode {
  PARSE_SINGLE_LINE = 1,
  PARSE_MULTI_LINE  = 2
} ParseMode;

typedef enum e_ques_cmd {
  QUES_INVALID = 0,
  QUES_BEGIN   = 1,
  QUES_END     = 2
} QuesCmd;

typedef struct st_parsed_part_cache {
  Slice slice;
  _Bool isString;
} ParsedPartCache;

typedef struct st_parse_context {
  pl2b_Program program;
  pl2b_Cmd *listTail;

  char *src;
  uint32_t srcIdx;
  ParseMode mode;

  pl2b_SourceInfo sourceInfo;

  uint32_t parseBufferSize;
  uint32_t parseBufferUsage;
  ParsedPartCache parseBuffer[0];
} ParseContext;

static ParseContext *createParseContext(char *src,
                                        uint16_t parseBufferSize);
static void parseLine(ParseContext *ctx, pl2b_Error *error);
static void parseQuesMark(ParseContext *ctx, pl2b_Error *error);
static void parsePart(ParseContext *ctx, pl2b_Error *error);
static Slice parseId(ParseContext *ctx, pl2b_Error *error);
static Slice parseStr(ParseContext *ctx, pl2b_Error *error);
static void checkBufferSize(ParseContext *ctx, pl2b_Error *error);
static void finishLine(ParseContext *ctx, pl2b_Error *error);
static pl2b_Cmd *cmdFromSlices2(pl2b_SourceInfo sourceInfo,
                                ParsedPartCache *parts);
static pl2b_Cmd *cmdFromSlices5(pl2b_Cmd *prev,
                                pl2b_Cmd *next,
                                void *extraData,
                                pl2b_SourceInfo sourceInfo,
                                ParsedPartCache *parts);
static void skipWhitespace(ParseContext *ctx);
static void skipComment(ParseContext *ctx);
static char curChar(ParseContext *ctx);
static char *curCharPos(ParseContext *ctx);
static void nextChar(ParseContext *ctx);
static _Bool isIdChar(char ch);
static _Bool isLineEnd(char ch);
static char *shrinkConv(char *start, char *end);

pl2b_Program pl2b_parse(char *source,
                        uint16_t parseBufferSize,
                        pl2b_Error *error) {
  ParseContext *context = createParseContext(source, parseBufferSize);
  if (context == NULL) {
    pl2b_errPrintf(error,
                   PL2B_ERR_MALLOC,
                   (pl2b_SourceInfo) {},
                   NULL,
                   "allocation failure");
    return (pl2b_Program) { NULL };
  }

  while (curChar(context) != '\0') {
    parseLine(context, error);
    if (pl2b_isError(error)) {
      break;
    }
  }

  pl2b_Program ret = context->program;
  free(context);
  return ret;
}

static ParseContext *createParseContext(char *src,
                                        uint16_t parseBufferSize) {
  if (parseBufferSize == 0) {
    return NULL;
  }

  ParseContext *ret = (ParseContext*)malloc(
    sizeof(ParseContext) + parseBufferSize * sizeof(Slice)
  );
  if (ret == NULL) {
    return NULL;
  }

  pl2b_initProgram(&ret->program);
  ret->listTail = NULL;
  ret->src = src;
  ret->srcIdx = 0;
  ret->sourceInfo = pl2b_sourceInfo("<unknown-file>", 1);
  ret->mode = PARSE_SINGLE_LINE;

  ret->parseBufferSize = parseBufferSize;
  ret->parseBufferUsage = 0;
  memset(ret->parseBuffer, 0, parseBufferSize * sizeof(Slice));
  return ret;
}

static void parseLine(ParseContext *ctx, pl2b_Error *error) {
  if (curChar(ctx) == '?') {
    parseQuesMark(ctx, error);
    if (pl2b_isError(error)) {
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
        pl2b_errPrintf(error, PL2B_ERR_UNCLOSED_BEGIN, ctx->sourceInfo,
                       NULL, "unclosed `?begin` block");
      }
      if (curChar(ctx) == '\n') {
        nextChar(ctx);
      }
      return;
    } else if (curChar(ctx) == '#') {
      skipComment(ctx);
    } else {
      parsePart(ctx, error);
      if (pl2b_isError(error)) {
        return;
      }
    }
  }
}

static void parseQuesMark(ParseContext *ctx, pl2b_Error *error) {
  assert(curChar(ctx) == '?');
  nextChar(ctx);

  char *start = curCharPos(ctx);
  while (isalnum((int)curChar(ctx))) {
    nextChar(ctx);
  }
  char *end = curCharPos(ctx);
  Slice s = slice(start, end);
  char *cstr = sliceIntoCStr(s);

  if (!strcmp(cstr, "begin")) {
    ctx->mode = PARSE_MULTI_LINE;
  } else if (!strcmp(cstr, "end")) {
    ctx->mode = PARSE_SINGLE_LINE;
    finishLine(ctx, error);
  } else {
    pl2b_errPrintf(error, PL2B_ERR_UNKNOWN_QUES, ctx->sourceInfo,
                   NULL, "unknown question mark operator: `%s`", cstr);
  }
}

static void parsePart(ParseContext *ctx, pl2b_Error *error) {
  Slice slice;
  _Bool isString;
  if (curChar(ctx) == '"' || curChar(ctx) == '\'') {
    slice = parseStr(ctx, error);
    isString = 1;
  } else {
    slice = parseId(ctx, error);
    isString = 0;
  }
  if (pl2b_isError(error)) {
    return;
  }

  checkBufferSize(ctx, error);
  if (pl2b_isError(error)) {
    return;
  }

  ctx->parseBuffer[ctx->parseBufferUsage++] =
    (ParsedPartCache) { slice, isString };
}

static Slice parseId(ParseContext *ctx, pl2b_Error *error) {
  (void)error;
  char *start = curCharPos(ctx);
  while (isIdChar(curChar(ctx))) {
    nextChar(ctx);
  }
  char *end = curCharPos(ctx);
  return slice(start, end);
}

static Slice parseStr(ParseContext *ctx, pl2b_Error *error) {
  assert(curChar(ctx) == '"' || curChar(ctx) == '\'');
  nextChar(ctx);

  char *start = curCharPos(ctx);
  while (curChar(ctx) != '"'
         && curChar(ctx) != '\''
         && !isLineEnd(curChar(ctx))) {
    if (curChar(ctx) == '\\') {
      nextChar(ctx);
      nextChar(ctx);
    } else {
      nextChar(ctx);
    }
  }
  char *end = curCharPos(ctx);
  end = shrinkConv(start, end);

  if (curChar(ctx) == '"' || curChar(ctx) == '\'') {
    nextChar(ctx);
  } else {
    pl2b_errPrintf(error, PL2B_ERR_UNCLOSED_BEGIN, ctx->sourceInfo,
                   NULL, "unclosed string literal");
    return nullSlice();
  }
  return slice(start, end);
}

static void checkBufferSize(ParseContext *ctx, pl2b_Error *error) {
  if (ctx->parseBufferSize <= ctx->parseBufferUsage + 1) {
    pl2b_errPrintf(error, PL2B_ERR_UNCLOSED_BEGIN, ctx->sourceInfo,
                   NULL, "command parts exceed internal parsing buffer");
  }
}

static void finishLine(ParseContext *ctx, pl2b_Error *error) {
  (void)error;

  pl2b_SourceInfo sourceInfo = ctx->sourceInfo;
  nextChar(ctx);
  if (ctx->parseBufferUsage == 0) {
    return;
  }
  if (ctx->listTail == NULL) {
    assert(ctx->program.commands == NULL);
    ctx->program.commands =
      ctx->listTail = cmdFromSlices2(ctx->sourceInfo, ctx->parseBuffer);
  } else {
    ctx->listTail = cmdFromSlices5(ctx->listTail, NULL, NULL,
                                   ctx->sourceInfo, ctx->parseBuffer);
  }
  if (ctx->listTail == NULL) {
    pl2b_errPrintf(error, PL2B_ERR_MALLOC, sourceInfo, 0,
                   "failed allocating pl2b_Cmd");
  }
  memset(ctx->parseBuffer, 0, sizeof(Slice) * ctx->parseBufferSize);
  ctx->parseBufferUsage = 0;
}

static pl2b_Cmd *cmdFromSlices2(pl2b_SourceInfo sourceInfo,
                                ParsedPartCache *parts) {
  return cmdFromSlices5(NULL, NULL, NULL, sourceInfo, parts);
}

static pl2b_Cmd *cmdFromSlices5(pl2b_Cmd *prev,
                                pl2b_Cmd *next,
                                void *extraData,
                                pl2b_SourceInfo sourceInfo,
                                ParsedPartCache *parts) {
  uint16_t partCount = 0;
  for (; !isNullSlice(parts[partCount].slice); ++partCount);

  pl2b_Cmd *ret = (pl2b_Cmd*)malloc(sizeof(pl2b_Cmd)
                                    + partCount * sizeof(pl2b_CmdPart));
  if (ret == NULL) {
    return NULL;
  }

  ret->prev = prev;
  if (prev != NULL) {
    prev->next = ret;
  }
  ret->next = next;
  if (next != NULL) {
    next->prev = ret;
  }
  ret->extraData = extraData;
  ret->resolveCache = NULL;
  ret->sourceInfo = sourceInfo;
  ret->cmd = pl2b_cmdPart(sliceIntoCStr(parts[0].slice),
                          parts[0].isString);
  for (uint16_t i = 1; i < partCount; i++) {
    ret->args[i - 1] = pl2b_cmdPart(sliceIntoCStr(parts[i].slice),
                                    parts[i].isString);
  }
  ret->args[partCount - 1] = pl2b_cmdPart(NULL, 0);
  return ret;
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
      ctx->sourceInfo.line += 1;
    }
    ctx->srcIdx += 1;
  }
}

static _Bool isIdChar(char ch) {
  unsigned char uch = transmuteU8(ch);
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

/*** -------------------- Semantic-ver parsing  -------------------- ***/

static const char *parseUint16(const char *src,
                               uint16_t *output,
                               pl2b_Error *error);

static void parseSemVerPostfix(const char *src,
                               char *output,
                               pl2b_Error *error);

pl2b_SemVer pl2b_zeroVersion(void) {
  pl2b_SemVer ret;
  ret.major = 0;
  ret.minor = 0;
  ret.patch = 0;
  memset(ret.postfix, 0, PL2B_SEMVER_POSTFIX_LEN);
  ret.exact = 0;
  return ret;
}

_Bool pl2b_isZeroVersion(pl2b_SemVer ver) {
  return ver.major == 0
         && ver.minor == 0
         && ver.patch == 0
         && ver.postfix[0] == 0
         && ver.exact == 0;
}

_Bool pl2b_isAlpha(pl2b_SemVer ver) {
  return ver.postfix[0] != '\0';
}

_Bool pl2b_isStable(pl2b_SemVer ver) {
  return !pl2b_isAlpha(ver) && ver.major != 0;
}

pl2b_SemVer pl2b_parseSemVer(const char *src, pl2b_Error *error) {
  pl2b_SemVer ret = pl2b_zeroVersion();
  if (src[0] == '^') {
    ret.exact = 1;
    src++;
  }

  src = parseUint16(src, &ret.major, error);
  if (pl2b_isError(error)) {
    pl2b_errPrintf(error, PL2B_ERR_SEMVER_PARSE,
                   pl2b_sourceInfo(NULL, 0), NULL,
                   "missing major version");
    goto done;
  } else if (src[0] == '\0') {
    goto done;
  } else if (src[0] == '-') {
    goto parse_postfix;
  } else if (src[0] != '.') {
    pl2b_errPrintf(error, PL2B_ERR_SEMVER_PARSE,
                   pl2b_sourceInfo(NULL, 0), NULL,
                   "expected `.`, got `%c`", src[0]);
    goto done;
  }

  src++;
  src = parseUint16(src, &ret.minor, error);
  if (pl2b_isError(error)) {
    pl2b_errPrintf(error, PL2B_ERR_SEMVER_PARSE,
                   pl2b_sourceInfo(NULL, 0),
                   NULL, "missing minor version");
    goto done;
  } else if (src[0] == '\0') {
    goto done;
  } else if (src[0] == '-') {
    goto parse_postfix;
  } else if (src[0] != '.') {
    pl2b_errPrintf(error, PL2B_ERR_SEMVER_PARSE,
                   pl2b_sourceInfo(NULL, 0), NULL,
                   "expected `.`, got `%c`", src[0]);
    goto done;
  }

  src++;
  src = parseUint16(src, &ret.patch, error);
  if (pl2b_isError(error)) {
    pl2b_errPrintf(error, PL2B_ERR_SEMVER_PARSE,
                   pl2b_sourceInfo(NULL, 0), NULL,
                   "missing patch version");
    goto done;
  } else if (src[0] == '\0') {
    goto done;
  } else if (src[0] != '-') {
    pl2b_errPrintf(error, PL2B_ERR_SEMVER_PARSE,
                   pl2b_sourceInfo(NULL, 0), NULL,
                   "unterminated semver, "
                   "expected `-` or `\\0`, got `%c`",
                   src[0]);
    goto done;
  }

parse_postfix:
  parseSemVerPostfix(src, ret.postfix, error);

done:
  return ret;
}

_Bool pl2b_isCompatible(pl2b_SemVer expected, pl2b_SemVer actual) {
  if (strncmp(expected.postfix,
              actual.postfix,
              PL2B_SEMVER_POSTFIX_LEN) != 0) {
    return 0;
  }
  if (expected.exact) {
    return expected.major == actual.major
           && expected.minor == actual.minor
           && expected.patch == actual.patch;
  } else if (expected.major == actual.major) {
    return (expected.minor == actual.minor
              && expected.patch < actual.patch)
            || (expected.minor < actual.minor);
  } else {
    return 0;
  }
}

pl2b_CmpResult pl2b_semverCmp(pl2b_SemVer ver1, pl2b_SemVer ver2) {
  if (!strncmp(ver1.postfix, ver2.postfix, PL2B_SEMVER_POSTFIX_LEN)) {
    return PL2B_CMP_NONE;
  }

  if (ver1.major < ver2.major) {
    return PL2B_CMP_LESS;
  } else if (ver1.major > ver2.major) {
    return PL2B_CMP_GREATER;
  } else if (ver1.minor < ver2.minor) {
    return PL2B_CMP_LESS;
  } else if (ver1.minor > ver2.minor) {
    return PL2B_CMP_GREATER;
  } else if (ver1.patch < ver2.patch) {
    return PL2B_CMP_LESS;
  } else if (ver1.patch > ver2.patch) {
    return PL2B_CMP_GREATER;
  } else {
    return PL2B_CMP_EQ;
  }
}

void pl2b_semverToString(pl2b_SemVer ver, char *buffer) {
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

static const char *parseUint16(const char *src,
                               uint16_t *output,
                               pl2b_Error *error) {
  if (!isdigit((int)src[0])) {
    pl2b_errPrintf(error, PL2B_ERR_SEMVER_PARSE,
                   pl2b_sourceInfo(NULL, 0),
                   NULL, "expected numeric version");
    return NULL;
  }
  *output = 0;
  while (isdigit((int)src[0])) {
    *output *= 10;
    *output += src[0] - '0';
    ++src;
  }
  return src;
}

static void parseSemVerPostfix(const char *src,
                               char *output,
                               pl2b_Error *error) {
  assert(src[0] == '-');
  ++src;
  if (src[0] == '\0') {
    pl2b_errPrintf(error, PL2B_ERR_SEMVER_PARSE,
                   pl2b_sourceInfo(NULL, 0), NULL,
                   "empty semver postfix");
    return;
  }
  for (size_t i = 0; i < PL2B_SEMVER_POSTFIX_LEN - 1; i++) {
    if (!(*output++ = *src++)) {
      return;
    }
  }
  if (src[0] != '\0') {
    pl2b_errPrintf(error, PL2B_ERR_SEMVER_PARSE,
                   pl2b_sourceInfo(NULL, 0), NULL,
                   "semver postfix too long");
    return;
  }
}

/*** ----------------------------- Run ----------------------------- ***/

typedef struct st_run_context {
  pl2b_Program *program;
  pl2b_Cmd *curCmd;
  void *userContext;

  void *libHandle;
  pl2b_Language *language;
} RunContext;

static RunContext *createRunContext(pl2b_Program *program);
static void destroyRunContext(RunContext *context);
static _Bool cmdHandler(RunContext *context,
                        pl2b_Cmd *cmd,
                        pl2b_Error *error);
static _Bool checkNextCmdRet(RunContext *context,
                             pl2b_Cmd *nextCmd,
                             pl2b_Error *error);
static _Bool loadLanguage(RunContext *context,
                          pl2b_Cmd *cmd,
                          pl2b_Error *error);

void pl2b_run(pl2b_Program *program, pl2b_Error *error) {
  RunContext *context = createRunContext(program);
  if (context == NULL) {
    pl2b_errPrintf(error, PL2B_ERR_MALLOC, pl2b_sourceInfo(NULL, 0),
                   NULL, "run: cannot allocate memory for run context");
    return;
  }

  while (cmdHandler(context, context->curCmd, error)) {
    if (pl2b_isError(error)) {
      break;
    }
  }

  destroyRunContext(context);
}

static RunContext *createRunContext(pl2b_Program *program) {
  RunContext *context = (RunContext*)malloc(sizeof(RunContext));
  if (context == NULL) {
    return NULL;
  }

  context->program = program;
  context->curCmd = program->commands;
  context->userContext = NULL;
  context->libHandle = NULL;
  context->language = NULL;
  return context;
}

static void destroyRunContext(RunContext *context) {
  if (context->libHandle != NULL) {
    if (context->language != NULL) {
      if (context->language->atExit != NULL) {
        context->language->atExit(context->userContext);
      }
      if (context->language->cmdCleanup != NULL) {
        for (pl2b_Cmd *cmd = context->program->commands;
             cmd != NULL;
             cmd = cmd->next) {
          context->language->cmdCleanup(cmd->extraData);
        }
      }
      context->language = NULL;
    }
    if (dlclose(context->libHandle) != 0) {
      fprintf(stderr, "[int/e] error invoking dlclose: %s\n", dlerror());
    }
  }
  free(context);
}

static _Bool cmdHandler(RunContext *context,
                        pl2b_Cmd *cmd,
                        pl2b_Error *error) {
  if (cmd == NULL) {
    return 0;
  }

  if (!strcmp(cmd->cmd.str, "language")) {
    return loadLanguage(context, cmd, error);
  } else if (!strcmp(cmd->cmd.str, "abort")) {
    return 0;
  }

  if (context->language == NULL) {
    pl2b_errPrintf(error, PL2B_ERR_NO_LANG, cmd->sourceInfo, NULL,
                   "no language loaded to execute user command");
    return 1;
  }

  if (cmd->resolveCache) {
    pl2b_PCallCmdStub *stub = (pl2b_PCallCmdStub*)cmd->resolveCache;
    if (stub == NULL) {
      context->curCmd = cmd->next;
      return 1;
    }
    pl2b_Cmd *nextCmd =
      stub(context->program, context->userContext, cmd, error);
    return checkNextCmdRet(context, nextCmd, error);
  }

  for (pl2b_PCallCmd *iter = context->language->pCallCmds;
       iter != NULL && !PL2B_EMPTY_CMD(iter);
       ++iter) {
    if (!iter->removed && !strcmp(cmd->cmd.str, iter->cmdName)) {
      if (iter->cmdName != NULL
          && strcmp(cmd->cmd.str, iter->cmdName) != 0) {
        /* Do nothing if so */
      } else if (iter->routerStub != NULL
                 && !iter->routerStub(cmd->cmd)) {
        /* Do nothing if so */
      } else {
        if (iter->deprecated) {
          fprintf(stderr, "[int/w] using deprecated command: %s\n",
                  iter->cmdName);
        }

        cmd->resolveCache = iter->stub;

        if (iter->stub == NULL) {
          fprintf(stderr,
                  "[int/w] entry for command %s exists but NULL\n",
                  cmd->cmd.str);
          context->curCmd = cmd->next;
          return 1;
        }

        pl2b_Cmd *nextCmd = iter->stub(context->program,
                                       context->userContext,
                                       cmd,
                                       error);
        return checkNextCmdRet(context, nextCmd, error);
      }
    }
  }

  if (context->language->fallback == NULL) {
    pl2b_errPrintf(error, PL2B_ERR_UNKNOWN_CMD, cmd->sourceInfo, NULL,
                   "`%s` is not recognized as an internal or external "
                   "command, operable program or batch file",
                   cmd->cmd);
    return 0;
  }

  pl2b_Cmd *nextCmd = context->language->fallback(
    context->program,
    context->userContext,
    cmd,
    error
  );

  return checkNextCmdRet(context, nextCmd, error);
}

static _Bool checkNextCmdRet(RunContext *context,
                             pl2b_Cmd *nextCmd,
                             pl2b_Error *error) {
  if (pl2b_isError(error)) {
    return 0;
  }
  if (nextCmd == NULL) {
    return 0;
  }

  context->curCmd = nextCmd;
  return 1;
}

static _Bool loadLanguage(RunContext *context,
                          pl2b_Cmd *cmd,
                          pl2b_Error *error) {
  if (context->language != NULL) {
    pl2b_errPrintf(error, PL2B_ERR_LOAD_LANG, cmd->sourceInfo, NULL,
                   "language: another language already loaded");
    return 0;
  }

  uint16_t argsLen = pl2b_argsLen(cmd);
  if (argsLen != 2) {
    pl2b_errPrintf(error, PL2B_ERR_LOAD_LANG, cmd->sourceInfo, NULL,
                   "language: expected 2 arguments, got %u",
                   argsLen - 1);
    return 0;
  }

  const char *langId = cmd->args[0].str;
  pl2b_SemVer langVer = pl2b_parseSemVer(cmd->args[1].str, error);
  if (pl2b_isError(error)) {
    return 0;
  }

  static char buffer[4096] = "./lib";
  strcat(buffer, langId);
  strcat(buffer, ".so");
  context->libHandle = dlopen(buffer, RTLD_NOW);
  if (context->libHandle == NULL) {
    char *pl2Home = getenv("PL2B_HOME");
    if (pl2Home != NULL) {
      strcpy(buffer, pl2Home);
      strcat(buffer, "/lib");
      strcat(buffer, langId);
      strcat(buffer, ".so");
      context->libHandle = dlopen(buffer, RTLD_NOW);
    }
  }

  if (context->libHandle == NULL) {
    pl2b_errPrintf(error, PL2B_ERR_LOAD_LANG, cmd->sourceInfo, NULL,
                   "language: cannot load language library `%s`: %s",
                   langId, dlerror());
    return 0;
  }

  void *loadPtr = dlsym(context->libHandle, "pl2ext_loadLanguage");
  if (loadPtr == NULL) {
      pl2b_errPrintf(error, PL2B_ERR_LOAD_LANG, cmd->sourceInfo, NULL,
                     "language: cannot locate `%s` "
                     "on library `%s`: %s",
                     "pl2ext_loadLanguage", langId, dlerror());
      return 0;
  }

  pl2b_LoadLanguage *load = (pl2b_LoadLanguage*)loadPtr;
  context->language = load(langVer, error);
  if (pl2b_isError(error)) {
    error->sourceInfo = cmd->sourceInfo;
    return 0;
  }

  if (context->language != NULL && context->language->init != NULL) {
    context->userContext = context->language->init(error);
    if (pl2b_isError(error)) {
      error->sourceInfo = cmd->sourceInfo;
      return 0;
    }
  }

  context->curCmd = cmd->next;
  return 1;
}
