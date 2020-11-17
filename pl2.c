#include "pl2.h"

#include <assert.h>
#include <ctype.h>
#include <dlfcn.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/*** ----------------- Transmute any char to uchar ----------------- ***/

static unsigned char transmuteU8(char i8) {
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
  return (size_t)(slice.end - slice.start);
}

_Bool pl2_isNullSlice(pl2_Slice slice) {
  return slice.start == slice.end;
}

/*** ----------------- Implementation of pl2_Error ----------------- ***/

pl2_Error *pl2_errorBuffer(size_t strBufferSize) {
  pl2_Error *ret = (pl2_Error*)malloc(sizeof(pl2_Error) + strBufferSize);
  memset(ret, 0, sizeof(pl2_Error) + strBufferSize);
  return ret;
}

void pl2_fillError(pl2_Error *error,
                   uint16_t errorCode,
                   uint16_t line,
                   const char *reason,
                   void *extraData) {
  assert(errorCode != 0 && "zero indicates non-error");
  error->extraData = extraData;
  error->errorCode = errorCode;
  error->line = line;
  strcpy(error->reason, reason);
}

void pl2_errPrintf(pl2_Error *error,
                   uint16_t errorCode,
                   uint16_t line,
                   void *extraData,
                   const char *fmt,
                   ...) {
  error->errorCode = errorCode;
  error->extraData = extraData;
  error->line = line;
  va_list ap;
  va_start(ap, fmt);
  vsprintf(error->reason, fmt, ap);
  va_end(ap);
}

void pl2_dropError(pl2_Error *error) {
  if (error->extraData) {
    free(error->extraData);
  }
  free(error);
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

pl2_Cmd *pl2_cmd2(uint16_t line, pl2_CmdPart *parts) {
  return pl2_cmd5(NULL, NULL, NULL, line, parts);
}

pl2_Cmd *pl2_cmd5(pl2_Cmd *prev,
                  pl2_Cmd *next,
                  void *extraData,
                  uint16_t line,
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
  ret->line = line;
  memset(ret->parts + partCount, 0, sizeof(pl2_CmdPart));
  return ret;
}

uint16_t pl2_cmdPartsLen(pl2_Cmd *cmd) {
  uint16_t acc = 0;
  pl2_CmdPart *iter = cmd->parts;
  while (!PL2_EMPTY_PART(iter)) {
    iter++;
    acc++;
  }
  return acc;
}

/*** ---------------- Implementation of pl2_Program --------------- ***/

void pl2_initProgram(pl2_Program *program) {
  program->commands = NULL;
}

void pl2_dropProgram(pl2_Program *program) {
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
        pl2_fillError(error, PL2_ERR_UNCLOSED_BEGIN, ctx->line,
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
    pl2_errPrintf(error, PL2_ERR_UNKNOWN_QUES, ctx->line, NULL,
                  "unknown question mark operator: `%s`",
                  pl2_unsafeIntoCStr(slice));
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
    pl2_fillError(error, PL2_ERR_UNCLOSED_STR, ctx->line,
                  "unclosed string literal", NULL);
    return pl2_nullSlice();
  }
  return pl2_slice(start, end);
}

static void checkBufferSize(ParseContext *ctx, pl2_Error *error) {
  if (ctx->partBufferSize <= ctx->partUsage + 1) {
    pl2_fillError(error, PL2_ERR_PARTBUF, ctx->line,
                  "command parts exceed internal parse buffer",
                  NULL);
  }
}

static void finishLine(ParseContext *ctx, pl2_Error *error) {
  (void)error;
  if (ctx->partUsage == 0) {
    return;
  }
  if (ctx->listTail == NULL) {
    assert(ctx->program.commands == NULL);
    ctx->program.commands = 
      ctx->listTail = pl2_cmd2(ctx->line, ctx->partBuffer);
  } else {
    ctx->listTail = pl2_cmd5(ctx->listTail,
                             NULL,
                             NULL,
                             ctx->line,
                             ctx->partBuffer);
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
                               pl2_Error *error);

static void parseSemVerPostfix(const char *src,
                               char *output,
                               pl2_Error *error);

pl2_SemVer pl2_zeroVersion(void) {
  pl2_SemVer ret;
  ret.major = 0;
  ret.minor = 0;
  ret.patch = 0;
  memset(ret.postfix, 0, PL2_SEMVER_POSTFIX_LEN);
  ret.exact = 0;
  return ret;
}

_Bool pl2_isZeroVersion(pl2_SemVer ver) {
  return ver.major == 0
         && ver.minor == 0
         && ver.patch == 0
         && ver.postfix[0] == 0
         && ver.exact == 0;
}

_Bool pl2_isAlpha(pl2_SemVer ver) {
  return ver.postfix[0] != '\0';
}

_Bool pl2_isStable(pl2_SemVer ver) {
  return !pl2_isAlpha(ver) && ver.major != 0;
}

pl2_SemVer pl2_parseSemVer(const char *src, pl2_Error *error) {
  pl2_SemVer ret = pl2_zeroVersion();
  if (src[0] == '^') {
    ret.exact = 1;
    src++;
  }
  
  src = parseUint16(src, &ret.major, error);
  if (pl2_isError(error)) {
    pl2_fillError(error, PL2_ERR_SEMVER_PARSE, 0,
                  "missing major version", NULL);
    goto done;
  } else if (src[0] == '\0') {
    goto done;
  } else if (src[0] == '-') {
    goto parse_postfix;
  } else if (src[0] != '.') {
    pl2_errPrintf(error, PL2_ERR_SEMVER_PARSE, 0, NULL,
                  "expected `.`, got `%c`", src[0]);
    goto done;
  }
  
  src++;
  src = parseUint16(src, &ret.minor, error);
  if (pl2_isError(error)) {
    pl2_fillError(error, PL2_ERR_SEMVER_PARSE, 0,
                  "missing minor version", NULL);
    goto done;
  } else if (src[0] == '\0') {
    goto done;
  } else if (src[0] == '-') {
    goto parse_postfix;
  } else if (src[0] != '.') {
    pl2_errPrintf(error, PL2_ERR_SEMVER_PARSE, 0, NULL,
                  "expected `.`, got `%c`", src[0]);
    goto done;
  }
  
  src++;
  src = parseUint16(src, &ret.patch, error);
  if (pl2_isError(error)) {
    pl2_fillError(error, PL2_ERR_SEMVER_PARSE, 0,
                  "missing patch version", NULL);
    goto done;
  } else if (src[0] == '\0') {
    goto done;
  } else if (src[0] != '-') {
    pl2_errPrintf(error, PL2_ERR_SEMVER_PARSE, 0, NULL,
                  "unterminated semver, expected `-` or `\\0`, got `%c`",
                  src[0]);
    goto done;
  } 
  
parse_postfix:
  parseSemVerPostfix(src, ret.postfix, error);
  
done:
  return ret;
}

_Bool pl2_isCompatible(pl2_SemVer require,pl2_SemVer given) {
  if (strncmp(require.postfix,
              given.postfix,
              PL2_SEMVER_POSTFIX_LEN) != 0) {
    return 0;
  }
  if (require.exact) {
    return require.major == given.major
           && require.minor == given.minor
           && require.patch == given.patch;
  } else if (require.major == given.major) {
    return (require.minor == given.minor && require.patch < given.patch)
           || (require.minor < given.minor);
  } else {
    return 0;
  }
}

pl2_CmpResult pl2_semverCmp(pl2_SemVer ver1, pl2_SemVer ver2) {
  if (!strncmp(ver1.postfix, ver2.postfix, PL2_SEMVER_POSTFIX_LEN)) {
    return PL2_CMP_NONE;
  }
  
  if (ver1.major < ver2.major) {
    return PL2_CMP_LESS;
  } else if (ver1.major > ver2.major) {
    return PL2_CMP_GREATER;
  } else if (ver1.minor < ver2.minor) {
    return PL2_CMP_LESS;
  } else if (ver1.minor > ver2.minor) {
    return PL2_CMP_GREATER;
  } else if (ver1.patch < ver2.patch) {
    return PL2_CMP_LESS;
  } else if (ver1.patch > ver2.patch) {
    return PL2_CMP_GREATER;
  } else {
    return PL2_CMP_EQ;
  }
}

void pl2_semverToString(pl2_SemVer ver, char *buffer) {
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
                               pl2_Error *error) {
  if (!isdigit(src[0])) {
    pl2_fillError(error, PL2_ERR_SEMVER_PARSE, 0,
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
    pl2_fillError(error, PL2_ERR_SEMVER_PARSE, 0,
                  "empty semver postfix", NULL);
    return;
  }
  for (size_t i = 0; i < PL2_SEMVER_POSTFIX_LEN - 1; i++) {
    if (!(*output++ = *src++)) {
      return;
    }
  }
  if (src[0] != '\0') {
    pl2_fillError(error, PL2_ERR_SEMVER_PARSE, 0,
                  "semver postfix too long", NULL);
    return;
  }
}

/*** ----------------------------- Run ----------------------------- ***/

typedef struct st_run_context {
  pl2_Program *program;
  pl2_Cmd *curCmd;
  void *userContext;
  
  void *libHandle;
  pl2_Language *language;
} RunContext;

static RunContext *createRunContext(pl2_Program *program);
static void destroyRunContext(RunContext *context);
static _Bool cmdHandler(RunContext *context,
                        pl2_Cmd *cmd,
                        pl2_Error *error);
static _Bool loadLanguage(RunContext *context,
                          pl2_Cmd *cmd,
                          pl2_Error *error);

void pl2_run(pl2_Program *program, pl2_Error *error) {
  RunContext *context = createRunContext(program);
  
  while (cmdHandler(context, context->curCmd, error)) {
    if (pl2_isError(error)) {
      fprintf(stderr, "at line %u: warn %u: %s",
              error->line, error->errorCode, error->reason);
    }
  }
  
  destroyRunContext(context);
}

static RunContext *createRunContext(pl2_Program *program) {
  RunContext *context = (RunContext*)malloc(sizeof(RunContext));
  context->program = program;
  context->curCmd = program->commands;
  context->userContext = NULL;
  context->libHandle = NULL;
  context->language = NULL;
  return context;
}

static void destroyRunContext(RunContext *context) {
  if (context->libHandle != NULL) {
    if (context->language->atExit != NULL) {
      context->language->atExit(context->userContext);
    }
    context->language = NULL;
    if (dlclose(context->libHandle) != 0) {
      fprintf(stderr,
              "[int/e] error invoking dlclose: %s\n",
              dlerror());
    }
  }
  free(context);
}

static _Bool cmdHandler(RunContext *context,
                        pl2_Cmd *cmd,
                        pl2_Error *error) {
  if (cmd == NULL) {
    return 0;
  }
                          
  if (pl2_isNullSlice(cmd->parts[0].prefix)) {
    pl2_Slice cmdBody = cmd->parts[0].body;
    if (pl2_sliceCmpCStr(cmdBody, "language")) {
      return loadLanguage(context, cmd, error);
    } else if (pl2_sliceCmpCStr(cmdBody, "abort")) {
      return 0;
    }
  }
  
  if (context->language == NULL) {
    pl2_errPrintf(error, PL2_ERR_NO_LANG, cmd->line, NULL,
                  "no language loaded to execute user command");
    return 1;
  }
  
  for (pl2_SInvokeCmd *iter = context->language->sinvokeCmds;
       iter != NULL && !PL2_EMPTY_CMD(iter);
       ++iter) {
    if (!iter->removed
        && pl2_sliceCmpCStr(cmd->parts[0].body, iter->cmdName)) {
      if (iter->deprecated) {
        fprintf(stderr, "[int/w] using deprecated command: %s\n",
                iter->cmdName);
      }
      const char *callBuffer[256] = { NULL };
      size_t i = 0;
      for (pl2_CmdPart *part = cmd->parts;
           !PL2_EMPTY_PART(part);
           ++part) {
        if (!pl2_isNullSlice(part->prefix)) {
          fprintf(stderr, "[int/w] sinvoke does not support prefix\n");
        }
        callBuffer[i] = pl2_unsafeIntoCStr(part->body);
        i++;
      }
      if (iter->stub != NULL) {
        iter->stub(callBuffer);
      }
      context->curCmd = cmd->next;
      return 1;
    }
  }
  
  for (pl2_PCallCmd *iter = context->language->pCallCmds;
       iter != NULL && !PL2_EMPTY_CMD(iter);
       ++iter) {
    if (!iter->removed
        && pl2_sliceCmpCStr(cmd->parts[0].body, iter->cmdName)) {
      if (iter->deprecated) {
        fprintf(stderr, "[int/w] using deprecated command: %s\n",
                iter->cmdName);
      }
      if (iter->stub == NULL) {
        context->curCmd = cmd->next;
        return 1;
      }
      
      pl2_Cmd *nextCmd = iter->stub(context->program,
                                    context->userContext,
                                    cmd,
                                    error);
      if (pl2_isError(error)) {
        return 0;
      }
      if (nextCmd == context->language->termCmd) {
        return 0;
      }
      context->curCmd = nextCmd ? nextCmd : cmd->next;
      return 1;
    }
  }
  
  if (context->language->fallback == NULL) {
    pl2_errPrintf(error, PL2_ERR_UNKNWON_CMD, cmd->line, NULL,
                  "`%s` is not recognized as an internal or external "
                  "command, operable program or batch file",
                  pl2_unsafeIntoCStr(cmd->parts[0].body));
    return 0;
  }
  
  pl2_Cmd *nextCmd = context->language->fallback(
    context->program,
    context->userContext,
    cmd,
    error
  );
  
  if (pl2_isError(error)) {
    return 0;
  }
  if (nextCmd == context->language->termCmd) {
    return 0;
  }
  
  context->curCmd = nextCmd ? nextCmd : cmd->next;
  return 1;
}

static _Bool loadLanguage(RunContext *context,
                          pl2_Cmd *cmd,
                          pl2_Error *error) {
  uint16_t partsLen = pl2_cmdPartsLen(cmd);
  if (partsLen != 3) {
    pl2_errPrintf(error, PL2_ERR_LOAD_LANG, cmd->line, NULL,
                  "language: expected 2 arguments, got %u",
                  partsLen - 1);
    return 0;
  }
  
  if (!pl2_isNullSlice(cmd->parts[1].prefix)
      || !pl2_isNullSlice(cmd->parts[2].prefix)) {
    pl2_errPrintf(error, PL2_ERR_LOAD_LANG, cmd->line, NULL,
                  "language: prefixing not allowed");
    return 0;
  }
  
  const char *langId = pl2_unsafeIntoCStr(cmd->parts[1].body);
  pl2_SemVer langVer = pl2_parseSemVer(
    pl2_unsafeIntoCStr(cmd->parts[2].body),
    error
  );
  if (pl2_isError(error)) {
    return 0;
  }
  
  static char buffer[4096] = "./lib";
  strcat(buffer, langId);
  strcat(buffer, ".so");
  context->libHandle = dlopen(buffer, RTLD_NOW);
  if (context->libHandle == NULL) {
    char *pl2Home = getenv("PL2_HOME");
    if (pl2Home != NULL) {
      strcpy(buffer, pl2Home);
      static char buffer[4096] = "/lib";
      strcat(buffer, langId);
      strcat(buffer, ".so");
      context->libHandle = dlopen(buffer, RTLD_NOW);
    }
  }
  
  if (context->libHandle == NULL) {
    pl2_errPrintf(error, PL2_ERR_LOAD_LANG, cmd->line, NULL,
                  "cannot load language library `%s`: %s",
                  langId, dlerror());
    return 0;
  }
  
  void *loadPtr = dlsym(
    context->libHandle,
    "pl2ext_loadLanguage"
  );
  if (loadPtr == NULL) {
    pl2_errPrintf(error, PL2_ERR_LOAD_LANG, cmd->line, NULL,
                  "cannot locate `%s` on library `%s`: %s",
                  "pl2ext_loadLanguage", langId, dlerror());
    return 0;
  }
  
  pl2_LoadLanguage *load = (pl2_LoadLanguage*)loadPtr;
  
  context->language = load(langVer, error);
  if (pl2_isError(error)) {
    return 0;
  }
  
  if (context->language->init != NULL) {
    context->userContext = context->language->init(error);
    if (pl2_isError(error)) {
      return 0;
    }
  }
  
  context->curCmd = cmd->next;
  return 1;
}
