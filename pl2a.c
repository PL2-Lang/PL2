#include "pl2a.h"

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

/*** ----------------- Implementation of pl2a_Error ---------------- ***/

pl2a_Error *pl2a_errorBuffer(uint16_t strBufferSize) {
  pl2a_Error *ret = (pl2a_Error*)malloc(sizeof(pl2a_Error) + strBufferSize);
  memset(ret, 0, sizeof(pl2a_Error) + strBufferSize);
  return ret;
}

void pl2a_errPrintf(pl2a_Error *error,
                    uint16_t errorCode,
                    pl2a_SourceInfo sourceInfo,
                    void *extraData,
                    const char *fmt,
                    ...) {
  error->errorCode = errorCode;
  error->extraData = extraData;
  error->sourceInfo = sourceInfo;
  va_list ap;
  va_start(ap, fmt);
  vsprintf(error->reason, fmt, ap);
  va_end(ap);
}

void pl2a_dropError(pl2a_Error *error) {
  if (error->extraData) {
    free(error->extraData);
  }
  free(error);
}

_Bool pl2a_isError(pl2a_Error *error) {
  return error->errorCode != 0;
}

/*** ------------------- Some toolkit functions -------------------- ***/

pl2a_SourceInfo pl2a_sourceInfo(const char *fileName, uint16_t line) {
  pl2a_SourceInfo ret;
  ret.fileName = fileName;
  ret.line = line;
  return ret;
}

pl2a_Cmd *pl2a_cmd3(pl2a_SourceInfo sourceInfo,
                    char *cmd,
                    char *args[]) {
  return pl2a_cmd6(NULL, NULL, NULL, sourceInfo, cmd, args);
}

pl2a_Cmd *pl2a_cmd6(pl2a_Cmd *prev,
                    pl2a_Cmd *next,
                    void *extraData,
                    pl2a_SourceInfo sourceInfo,
                    char *cmd,
                    char *args[]) {
  uint16_t argLen = 0;
  for (; args[argLen] != NULL; ++argLen);

  pl2a_Cmd *ret = (pl2a_Cmd*)malloc(sizeof(pl2a_Cmd) +
                                    (argLen + 1) * sizeof(char*));
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
  ret->extraData = extraData;
  for (uint16_t i = 0; i < argLen; i++) {
    ret->args[i] = args[i];
  }
  ret->args[argLen] = NULL;
  return ret;
}

uint16_t pl2a_argsLen(pl2a_Cmd *cmd) {
  uint16_t acc = 0;
  char **iter = cmd->args;
  while (*iter != NULL) {
    iter++;
    acc++;
  }
  return acc;
}

/*** ---------------- Implementation of pl2a_Program --------------- ***/

void pl2a_initProgram(pl2a_Program *program) {
  program->commands = NULL;
}

void pl2a_dropProgram(pl2a_Program *program) {
  pl2a_Cmd *iter = program->commands;
  while (iter != NULL) {
    pl2a_Cmd *next = iter->next;
    free(iter);
    iter = next;
  }
}

void pl2a_debugPrintProgram(const pl2a_Program *program) {
  fprintf(stderr, "program commands\n");
  pl2a_Cmd *cmd = program->commands;
  while (cmd != NULL) {
    fprintf(stderr, "\t%s [", cmd->cmd);
    for (uint16_t i = 0; cmd->args[i] != NULL; i++) {
      char *arg = cmd->args[i];
      fprintf(stderr, "`%s`, ", arg);
    }
    fprintf(stderr, "\b\b]\n");
    cmd = cmd->next;
  }
  fprintf(stderr, "end program commands\n");
}

/*** ----------------- Implementation of pl2a_parse ---------------- ***/

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
  pl2a_Program program;
  pl2a_Cmd *listTail;

  char *src;
  uint32_t srcIdx;
  ParseMode mode;

  pl2a_SourceInfo sourceInfo;

  uint32_t partBufferSize;
  uint32_t partUsage;
  Slice partBuffer[0];
} ParseContext;

static ParseContext *createParseContext(char *src);
static void parseLine(ParseContext *ctx, pl2a_Error *error);
static void parseQuesMark(ParseContext *ctx, pl2a_Error *error);
static void parsePart(ParseContext *ctx, pl2a_Error *error);
static Slice parseId(ParseContext *ctx, pl2a_Error *error);
static Slice parseStr(ParseContext *ctx, pl2a_Error *error);
static void checkBufferSize(ParseContext *ctx, pl2a_Error *error);
static void finishLine(ParseContext *ctx, pl2a_Error *error);
static pl2a_Cmd *cmdFromSlices2(pl2a_SourceInfo sourceInfo,
                                Slice *parts);
static pl2a_Cmd *cmdFromSlices5(pl2a_Cmd *prev,
                                pl2a_Cmd *next,
                                void *extraData,
                                pl2a_SourceInfo sourceInfo,
                                Slice *parts);
static void skipWhitespace(ParseContext *ctx);
static void skipComment(ParseContext *ctx);
static char curChar(ParseContext *ctx);
static char *curCharPos(ParseContext *ctx);
static void nextChar(ParseContext *ctx);
static _Bool isIdChar(char ch);
static _Bool isLineEnd(char ch);
static char *shrinkConv(char *start, char *end);

pl2a_Program pl2a_parse(char *source, pl2a_Error *error) {
  ParseContext *context = createParseContext(source);

  while (curChar(context) != '\0') {
    parseLine(context, error);
    if (pl2a_isError(error)) {
      break;
    }
  }

  pl2a_Program ret = context->program;
  free(context);
  return ret;
}

static ParseContext *createParseContext(char *src) {
  ParseContext *ret = (ParseContext*)malloc(
    sizeof(ParseContext) + 512 * sizeof(Slice)
  );

  pl2a_initProgram(&ret->program);
  ret->listTail = NULL;
  ret->src = src;
  ret->srcIdx = 0;
  ret->sourceInfo = pl2a_sourceInfo("<unknown-file>", 1);
  ret->mode = PARSE_SINGLE_LINE;

  ret->partBufferSize = 512;
  ret->partUsage = 0;
  memset(ret->partBuffer, 0, 512 * sizeof(Slice));
  return ret;
}

static void parseLine(ParseContext *ctx, pl2a_Error *error) {
  if (curChar(ctx) == '?') {
    parseQuesMark(ctx, error);
    if (pl2a_isError(error)) {
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
        pl2a_errPrintf(error, PL2A_ERR_UNCLOSED_BEGIN, ctx->sourceInfo,
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
      if (pl2a_isError(error)) {
        return;
      }
    }
  }
}

static void parseQuesMark(ParseContext *ctx, pl2a_Error *error) {
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
    pl2a_errPrintf(error, PL2A_ERR_UNKNOWN_QUES, ctx->sourceInfo,
                   NULL, "unknown question mark operator: `%s`", cstr);
  }
}

static void parsePart(ParseContext *ctx, pl2a_Error *error) {
  Slice part;
  if (curChar(ctx) == '"' || curChar(ctx) == '\'') {
    part = parseStr(ctx, error);
  } else {
    part = parseId(ctx, error);
  }
  if (pl2a_isError(error)) {
    return;
  }

  checkBufferSize(ctx, error);
  if (pl2a_isError(error)) {
    return;
  }

  ctx->partBuffer[ctx->partUsage++] = part;
}

static Slice parseId(ParseContext *ctx, pl2a_Error *error) {
  (void)error;
  char *start = curCharPos(ctx);
  while (isIdChar(curChar(ctx))) {
    nextChar(ctx);
  }
  char *end = curCharPos(ctx);
  return slice(start, end);
}

static Slice parseStr(ParseContext *ctx, pl2a_Error *error) {
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
    pl2a_errPrintf(error, PL2A_ERR_UNCLOSED_BEGIN, ctx->sourceInfo,
                   NULL, "unclosed string literal");
    return nullSlice();
  }
  return slice(start, end);
}

static void checkBufferSize(ParseContext *ctx, pl2a_Error *error) {
  if (ctx->partBufferSize <= ctx->partUsage + 1) {
    pl2a_errPrintf(error, PL2A_ERR_UNCLOSED_BEGIN, ctx->sourceInfo,
                   NULL, "command parts exceed internal parsing buffer");
  }
}

static void finishLine(ParseContext *ctx, pl2a_Error *error) {
  (void)error;
  nextChar(ctx);
  if (ctx->partUsage == 0) {
    return;
  }
  if (ctx->listTail == NULL) {
    assert(ctx->program.commands == NULL);
    ctx->program.commands =
      ctx->listTail = cmdFromSlices2(ctx->sourceInfo, ctx->partBuffer);
  } else {
    ctx->listTail = cmdFromSlices5(ctx->listTail, NULL, NULL,
                                   ctx->sourceInfo, ctx->partBuffer);
  }
  memset(ctx->partBuffer, 0, sizeof(Slice) * ctx->partBufferSize);
  ctx->partUsage = 0;
}

static pl2a_Cmd *cmdFromSlices2(pl2a_SourceInfo sourceInfo,
                                Slice *parts) {
  return cmdFromSlices5(NULL, NULL, NULL, sourceInfo, parts);
}

static pl2a_Cmd *cmdFromSlices5(pl2a_Cmd *prev,
                                pl2a_Cmd *next,
                                void *extraData,
                                pl2a_SourceInfo sourceInfo,
                                Slice *parts) {
  uint16_t partCount = 0;
  for (; !isNullSlice(parts[partCount]); ++partCount);

  pl2a_Cmd *ret = (pl2a_Cmd*)malloc(sizeof(pl2a_Cmd) +
                                    partCount * sizeof(char*));
  ret->prev = prev;
  if (prev != NULL) {
    prev->next = ret;
  }
  ret->next = next;
  if (next != NULL) {
    next->prev = ret;
  }
  ret->extraData = extraData;
  ret->sourceInfo = sourceInfo;
  ret->cmd = sliceIntoCStr(parts[0]);
  for (uint16_t i = 1; i < partCount; i++) {
    ret->args[i - 1] = sliceIntoCStr(parts[i]);
  }
  ret->args[partCount - 1] = NULL;
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
                               pl2a_Error *error);

static void parseSemVerPostfix(const char *src,
                               char *output,
                               pl2a_Error *error);

pl2a_SemVer pl2a_zeroVersion(void) {
  pl2a_SemVer ret;
  ret.major = 0;
  ret.minor = 0;
  ret.patch = 0;
  memset(ret.postfix, 0, PL2A_SEMVER_POSTFIX_LEN);
  ret.exact = 0;
  return ret;
}

_Bool pl2a_isZeroVersion(pl2a_SemVer ver) {
  return ver.major == 0
         && ver.minor == 0
         && ver.patch == 0
         && ver.postfix[0] == 0
         && ver.exact == 0;
}

_Bool pl2a_isAlpha(pl2a_SemVer ver) {
  return ver.postfix[0] != '\0';
}

_Bool pl2a_isStable(pl2a_SemVer ver) {
  return !pl2a_isAlpha(ver) && ver.major != 0;
}

pl2a_SemVer pl2a_parseSemVer(const char *src, pl2a_Error *error) {
  pl2a_SemVer ret = pl2a_zeroVersion();
  if (src[0] == '^') {
    ret.exact = 1;
    src++;
  }
  
  src = parseUint16(src, &ret.major, error);
  if (pl2a_isError(error)) {
    pl2a_errPrintf(error, PL2A_ERR_SEMVER_PARSE,
                   pl2a_sourceInfo(NULL, 0), NULL,
                   "missing major version");
    goto done;
  } else if (src[0] == '\0') {
    goto done;
  } else if (src[0] == '-') {
    goto parse_postfix;
  } else if (src[0] != '.') {
    pl2a_errPrintf(error, PL2A_ERR_SEMVER_PARSE,
                   pl2a_sourceInfo(NULL, 0), NULL,
                   "expected `.`, got `%c`", src[0]);
    goto done;
  }
  
  src++;
  src = parseUint16(src, &ret.minor, error);
  if (pl2a_isError(error)) {
    pl2a_errPrintf(error, PL2A_ERR_SEMVER_PARSE,
                   pl2a_sourceInfo(NULL, 0),
                   NULL, "missing minor version");
    goto done;
  } else if (src[0] == '\0') {
    goto done;
  } else if (src[0] == '-') {
    goto parse_postfix;
  } else if (src[0] != '.') {
    pl2a_errPrintf(error, PL2A_ERR_SEMVER_PARSE,
                   pl2a_sourceInfo(NULL, 0), NULL,
                   "expected `.`, got `%c`", src[0]);
    goto done;
  }
  
  src++;
  src = parseUint16(src, &ret.patch, error);
  if (pl2a_isError(error)) {
    pl2a_errPrintf(error, PL2A_ERR_SEMVER_PARSE,
                   pl2a_sourceInfo(NULL, 0), NULL,
                   "missing patch version");
    goto done;
  } else if (src[0] == '\0') {
    goto done;
  } else if (src[0] != '-') {
    pl2a_errPrintf(error, PL2A_ERR_SEMVER_PARSE,
                   pl2a_sourceInfo(NULL, 0), NULL,
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

_Bool pl2a_isCompatible(pl2a_SemVer require,pl2a_SemVer given) {
  if (strncmp(require.postfix,
              given.postfix,
              PL2A_SEMVER_POSTFIX_LEN) != 0) {
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

pl2a_CmpResult pl2a_semverCmp(pl2a_SemVer ver1, pl2a_SemVer ver2) {
  if (!strncmp(ver1.postfix, ver2.postfix, PL2A_SEMVER_POSTFIX_LEN)) {
    return PL2A_CMP_NONE;
  }
  
  if (ver1.major < ver2.major) {
    return PL2A_CMP_LESS;
  } else if (ver1.major > ver2.major) {
    return PL2A_CMP_GREATER;
  } else if (ver1.minor < ver2.minor) {
    return PL2A_CMP_LESS;
  } else if (ver1.minor > ver2.minor) {
    return PL2A_CMP_GREATER;
  } else if (ver1.patch < ver2.patch) {
    return PL2A_CMP_LESS;
  } else if (ver1.patch > ver2.patch) {
    return PL2A_CMP_GREATER;
  } else {
    return PL2A_CMP_EQ;
  }
}

void pl2a_semverToString(pl2a_SemVer ver, char *buffer) {
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
                               pl2a_Error *error) {
  if (!isdigit((int)src[0])) {
    pl2a_errPrintf(error, PL2A_ERR_SEMVER_PARSE,
                   pl2a_sourceInfo(NULL, 0),
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
                               pl2a_Error *error) {
  assert(src[0] == '-');
  ++src;
  if (src[0] == '\0') {
    pl2a_errPrintf(error, PL2A_ERR_SEMVER_PARSE,
                   pl2a_sourceInfo(NULL, 0), NULL,
                   "empty semver postfix");
    return;
  }
  for (size_t i = 0; i < PL2A_SEMVER_POSTFIX_LEN - 1; i++) {
    if (!(*output++ = *src++)) {
      return;
    }
  }
  if (src[0] != '\0') {
    pl2a_errPrintf(error, PL2A_ERR_SEMVER_PARSE,
                   pl2a_sourceInfo(NULL, 0), NULL,
                   "semver postfix too long");
    return;
  }
}

/*** ----------------------------- Run ----------------------------- ***/

typedef struct st_run_context {
  pl2a_Program *program;
  pl2a_Cmd *curCmd;
  void *userContext;
  
  void *libHandle;
  pl2a_Language *language;
  _Bool ownLanguage;
} RunContext;

static RunContext *createRunContext(pl2a_Program *program);
static void destroyRunContext(RunContext *context);
static _Bool cmdHandler(RunContext *context,
                        pl2a_Cmd *cmd,
                        pl2a_Error *error);
static _Bool loadLanguage(RunContext *context,
                          pl2a_Cmd *cmd,
                          pl2a_Error *error);
static pl2a_Language *ezLoad(void *libHandle,
                            const char **cmdNames,
                            pl2a_Error *error);

void pl2a_run(pl2a_Program *program, pl2a_Error *error) {
  RunContext *context = createRunContext(program);
  
  while (cmdHandler(context, context->curCmd, error)) {
    if (pl2a_isError(error)) {
      fprintf(stderr, "at line %u: warn %u: %s",
              error->sourceInfo.line, error->errorCode, error->reason);
    }
  }
  
  destroyRunContext(context);
}

static RunContext *createRunContext(pl2a_Program *program) {
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
    if (context->language != NULL) {
      if (context->language->atExit != NULL) {
        context->language->atExit(context->userContext);
      }
      if (context->ownLanguage) {
        free(context->language->sinvokeCmds);
        free(context->language->pCallCmds);
        free(context->language);
      }
      context->language = NULL;
    }
    if (dlclose(context->libHandle) != 0) {
      fprintf(stderr,
              "[int/e] error invoking dlclose: %s\n",
              dlerror());
    }
  }
  free(context);
}

static _Bool cmdHandler(RunContext *context,
                        pl2a_Cmd *cmd,
                        pl2a_Error *error) {
  if (cmd == NULL) {
    return 0;
  }

  if (!strcmp(cmd->cmd, "language")) {
    return loadLanguage(context, cmd, error);
  } else if (!strcmp(cmd->cmd, "abort")) {
    return 0;
  }
  
  if (context->language == NULL) {
    pl2a_errPrintf(error, PL2A_ERR_NO_LANG, cmd->sourceInfo, NULL,
                  "no language loaded to execute user command");
    return 1;
  }
  
  for (pl2a_SInvokeCmd *iter = context->language->sinvokeCmds;
       iter != NULL && !PL2A_EMPTY_CMD(iter);
       ++iter) {
    if (!iter->removed && !strcmp(cmd->cmd, iter->cmdName)) {
      if (iter->deprecated) {
        fprintf(stderr, "[int/w] using deprecated command: %s\n",
                iter->cmdName);
      }
      if (iter->stub != NULL) {
        iter->stub((const char **)cmd->args);
      }
      context->curCmd = cmd->next;
      return 1;
    }
  }
  
  for (pl2a_PCallCmd *iter = context->language->pCallCmds;
       iter != NULL && !PL2A_EMPTY_CMD(iter);
       ++iter) {
    if (!iter->removed && !strcmp(cmd->cmd, iter->cmdName)) {
      if (iter->deprecated) {
        fprintf(stderr, "[int/w] using deprecated command: %s\n",
                iter->cmdName);
      }
      if (iter->stub == NULL) {
        context->curCmd = cmd->next;
        return 1;
      }
      
      pl2a_Cmd *nextCmd = iter->stub(context->program,
                                    context->userContext,
                                    cmd,
                                    error);
      if (pl2a_isError(error)) {
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
    pl2a_errPrintf(error, PL2A_ERR_UNKNWON_CMD, cmd->sourceInfo, NULL,
                   "`%s` is not recognized as an internal or external "
                   "command, operable program or batch file",
                   cmd->cmd);
    return 0;
  }
  
  pl2a_Cmd *nextCmd = context->language->fallback(
    context->program,
    context->userContext,
    cmd,
    error
  );
  
  if (nextCmd == context->language->termCmd) {
    pl2a_errPrintf(error, PL2A_ERR_UNKNWON_CMD, cmd->sourceInfo, NULL,
                  "`%s` is not recognized as an internal or external "
                  "command, operable program or batch file",
                   cmd->cmd);
    return 0;
  }
  
  if (pl2a_isError(error)) {
    return 0;
  }
  if (nextCmd == context->language->termCmd) {
    return 0;
  }
  
  context->curCmd = nextCmd ? nextCmd : cmd->next;
  return 1;
}

static _Bool loadLanguage(RunContext *context,
                          pl2a_Cmd *cmd,
                          pl2a_Error *error) {
  if (context->language != NULL) {
    pl2a_errPrintf(error, PL2A_ERR_LOAD_LANG, cmd->sourceInfo, NULL,
                  "language: another language already loaded");
    return 0;
  }

  uint16_t argsLen = pl2a_argsLen(cmd);
  if (argsLen != 2) {
    pl2a_errPrintf(error, PL2A_ERR_LOAD_LANG, cmd->sourceInfo, NULL,
                   "language: expected 2 arguments, got %u",
                   argsLen - 1);
    return 0;
  }
  
  const char *langId = cmd->args[0];
  pl2a_SemVer langVer = pl2a_parseSemVer(cmd->args[1], error);
  if (pl2a_isError(error)) {
    return 0;
  }
  
  static char buffer[4096] = "./lib";
  strcat(buffer, langId);
  strcat(buffer, ".so");
  context->libHandle = dlopen(buffer, RTLD_NOW);
  if (context->libHandle == NULL) {
    char *pl2Home = getenv("PL2A_HOME");
    if (pl2Home != NULL) {
      strcpy(buffer, pl2Home);
      strcat(buffer, "/lib");
      strcat(buffer, langId);
      strcat(buffer, ".so");
      context->libHandle = dlopen(buffer, RTLD_NOW);
    }
  }
  
  if (context->libHandle == NULL) {
    pl2a_errPrintf(error, PL2A_ERR_LOAD_LANG, cmd->sourceInfo, NULL,
                  "language: cannot load language library `%s`: %s",
                  langId, dlerror());
    return 0;
  }
  
  void *loadPtr = dlsym(
    context->libHandle,
    "pl2ext_loadLanguage"
  );
  if (loadPtr == NULL) {
    void *ezLoadPtr = dlsym(
      context->libHandle,
      "pl2ezload"
    );
    
    if (ezLoadPtr == NULL) {
      pl2a_errPrintf(error, PL2A_ERR_LOAD_LANG, cmd->sourceInfo, NULL,
                  "language: cannot locate `%s` or `%s` on library `%s`:"
                  " %s",
                  "pl2ext_loadLanguage", "pl2ezload", langId, dlerror());
      return 0;
    }
    
    pl2a_EasyLoadLanguage *load = (pl2a_EasyLoadLanguage*)ezLoadPtr;
    context->language = ezLoad(context->libHandle, load(), error);
    if (pl2a_isError(error)) {
      return 0;
    }
    context->ownLanguage = 1;
  } else {
    pl2a_LoadLanguage *load = (pl2a_LoadLanguage*)loadPtr;
    context->language = load(langVer, error);
    if (pl2a_isError(error)) {
      return 0;
    }
    context->ownLanguage = 0;
  }
  
  if (context->language != NULL && context->language->init != NULL) {
    context->userContext = context->language->init(error);
    if (pl2a_isError(error)) {
      return 0;
    }
  }

  context->curCmd = cmd->next;
  return 1;
}

static pl2a_Language *ezLoad(void *libHandle,
                            const char **cmdNames,
                            pl2a_Error *error) {
  if (cmdNames == NULL || cmdNames[0] == NULL) {
    return NULL;
  }
  uint16_t count = 0;
  for (const char **iter = cmdNames; *iter != NULL; iter++) {
    ++count;
  }
  
  pl2a_Language *ret = (pl2a_Language*)malloc(sizeof(pl2a_Language));
  ret->langName = "unknown";
  ret->langInfo = "anonymous language loaded by ezload";
  ret->termCmd = NULL;
  ret->init = NULL;
  ret->atExit = NULL;
  ret->pCallCmds = NULL;
  ret->fallback = NULL;
  ret->sinvokeCmds = (pl2a_SInvokeCmd*)malloc(
    sizeof(pl2a_SInvokeCmd) * (count + 1)
  );
  
  memset(ret->sinvokeCmds, 0, (count + 1) * sizeof(pl2a_SInvokeCmd));
  static char nameBuffer[512];
  for (uint16_t i = 0; i < count; i++) {
    const char *cmdName = cmdNames[i];
    strcpy(nameBuffer, "pl2ez_");
    strcat(nameBuffer, cmdName);
    void *ptr = dlsym(libHandle, nameBuffer);
    if (ptr == NULL) {
      pl2a_errPrintf(error, PL2A_ERR_LOAD_LANG, pl2a_sourceInfo(NULL, 0),
                     NULL,
                     "language: ezload: cannot load function `%s`: %s",
                     nameBuffer, dlerror());
      free(ret->sinvokeCmds);
      free(ret);
      return NULL;
    }

    ret->sinvokeCmds[i].cmdName = cmdName;
    ret->sinvokeCmds[i].stub = (pl2a_SInvokeCmdStub*)ptr;
    ret->sinvokeCmds[i].deprecated = 0;
    ret->sinvokeCmds[i].removed = 0;
  }

  return ret;
}
