#ifndef PLAPI_PL2_H
#define PLAPI_PL2_H

#define PLAPI

/*** ------------------------ configurations ----------------------- ***/

#define PL2_VER_MAJOR 0           /* Major version of PL2 */
#define PL2_VER_MINOR 1           /* Minor version of PL2 */
#define PL2_VER_PATCH 0           /* Patch version of PL2 */
#define PL2_VER_POSTFIX "alpha"   /* Version postfix */

/*** ---------------------- end configurations --------------------- ***/

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*** ------------------------ pl2_CmpResult ------------------------ ***/

typedef enum e_pl2_cmp_result {
  PL2_CMP_LESS    = -1, /* LHS < RHS */
  PL2_CMP_EQ      = 0,  /* LHS = RHS */
  PL2_CMP_GREATER = 1,  /* LHS > RHS */
  PL2_CMP_NONE    = 255 /* not comparable */
} pl2_CmpResult;

/*** -------------------------- pl2_Slice -------------------------- ***/

typedef struct st_pl2_slice {
  const char *start;
  const char *end;
} pl2_Slice;

pl2_Slice pl2_slice(const char *start, const char *end);
pl2_Slice pl2_nullSlice(void);
pl2_Slice pl2_sliceFromCStr(const char *cStr);
void pl2_printSlice(void *ptr, pl2_Slice slice);
char *pl2_unsafeIntoCStr(pl2_Slice slice);

_Bool pl2_sliceCmp(pl2_Slice s1, pl2_Slice s2);
_Bool pl2_sliceCmpCStr(pl2_Slice slice, const char *cStr);
size_t pl2_sliceLen(pl2_Slice slice);
_Bool pl2_isNullSlice(pl2_Slice slice);

/*** ------------------------- pl2_MString ------------------------- ***/

/*** -------------------------- pl2_Error -------------------------- ***/

typedef struct st_pl2_error {
  void *extraData;
  uint16_t errorCode;
  uint16_t line;
  char reason[0];
} pl2_Error;

typedef enum e_pl2_error_code {
  PL2_ERR_NONE           = 0,  /* no error */
  PL2_ERR_GENERAL        = 1,  /* general hard error */
  PL2_ERR_PARTBUF        = 2,  /* part buffer exceeded */
  PL2_ERR_UNCLOSED_STR   = 3,  /* unclosed string literal */
  PL2_ERR_UNCLOSED_BEGIN = 4,  /* unclosed ?begin block */
  PL2_ERR_EMPTY_CMD      = 5,  /* empty command */
  PL2_ERR_SEMVER_PARSE   = 6,  /* semver parse error */
  PL2_ERR_UNKNOWN_QUES   = 7,  /* unknown question mark command */
  PL2_ERR_LOAD_LANG      = 8,  /* error loading language */
  PL2_ERR_NO_LANG        = 9,  /* language not loaded */
  PL2_ERR_UNKNWON_CMD    = 10, /* unknown command */
  
  PL2_ERR_USER           = 100 /* generic user error */
} pl2_ErrorCode;

pl2_Error *pl2_errorBuffer(size_t strBufferSize);

void pl2_fillError(pl2_Error *error,
                   uint16_t errorCode,
                   uint16_t line,
                   const char *reason,
                   void *extraData);

void pl2_errPrintf(pl2_Error *error,
                   uint16_t errorCode,
                   uint16_t line,
                   void *extraData,
                   const char *fmt,
                   ...);

void pl2_dropError(pl2_Error *error);

_Bool pl2_isError(pl2_Error *error);

/*** ----------------------- pl2_SourceInfo  ----------------------- ***/

typedef struct st_pl2_source_info {
  const char *fileName;
  uint16_t line;
} pl2_SourceInfo;

pl2_SourceInfo pl2_sourceInfo(const char *fileName, uint16_t line);

/*** --------------------------- pl2_Cmd --------------------------- ***/

typedef struct st_pl2_cmd_part {
  pl2_Slice prefix;
  pl2_Slice body;
} pl2_CmdPart;

pl2_CmdPart pl2_cmdPart(pl2_Slice prefix, pl2_Slice body);

#define PL2_EMPTY_PART(part) \
  (pl2_isNullSlice((part)->prefix) && pl2_isNullSlice((part)->body))

typedef struct st_pl2_cmd {
  struct st_pl2_cmd *prev;
  struct st_pl2_cmd *next;

  void *extraData;
  uint16_t line;
  pl2_CmdPart parts[0];
} pl2_Cmd;

pl2_Cmd *pl2_cmd2(uint16_t line, pl2_CmdPart *parts);
pl2_Cmd *pl2_cmd5(pl2_Cmd *prev,
                  pl2_Cmd *next,
                  void *extraData,
                  uint16_t line,
                  pl2_CmdPart *parts);

uint16_t pl2_cmdPartsLen(pl2_Cmd *cmd);

/*** ------------------------- pl2_Program ------------------------- ***/

typedef struct st_pl2_program {
  pl2_Slice language;
  pl2_Cmd *commands;
} pl2_Program;

void pl2_initProgram(pl2_Program *program);
pl2_Program pl2_parse(char *source, pl2_Error *error);
void pl2_dropProgram(pl2_Program *program);
void pl2_debugPrintProgram(const pl2_Program *program);

/*** -------------------- Semantic-ver parsing  -------------------- ***/

#define PL2_SEMVER_POSTFIX_LEN 15

typedef struct st_pl2_sem_ver {
  uint16_t major;
  uint16_t minor;
  uint16_t patch;
  char postfix[PL2_SEMVER_POSTFIX_LEN];
  _Bool exact;
} pl2_SemVer;

pl2_SemVer pl2_zeroVersion(void);
pl2_SemVer pl2_parseSemVer(const char *src, pl2_Error *error);
_Bool pl2_isZeroVersion(pl2_SemVer ver);
_Bool pl2_isAlpha(pl2_SemVer ver);
_Bool pl2_isStable(pl2_SemVer ver);
_Bool pl2_isCompatible(pl2_SemVer ver1, pl2_SemVer ver2);
pl2_CmpResult pl2_semverCmp(pl2_SemVer ver1, pl2_SemVer ver2);
void pl2_semverToString(pl2_SemVer ver, char *buffer);

/*** ------------------------ pl2_Extension ------------------------ ***/

typedef void (pl2_SInvokeCmdStub)(const char *strings[]);
typedef pl2_Cmd* (pl2_PCallCmdStub)(pl2_Program *program,
                                    void *context,
                                    pl2_Cmd *command,
                                    pl2_Error *error);

typedef void* (pl2_InitStub)(pl2_Error *error);
typedef void (pl2_AtexitStub)(void *context);

typedef struct st_pl2_sinvoke_cmd {
  const char *cmdName;
  pl2_SInvokeCmdStub *stub;
  _Bool deprecated;
  _Bool removed;
} pl2_SInvokeCmd;

typedef struct st_pl2_pcall_func {
  const char *cmdName;
  pl2_PCallCmdStub *stub;
  _Bool deprecated;
  _Bool removed;
} pl2_PCallCmd;

#define PL2_EMPTY_CMD(cmd) ((cmd)->cmdName == 0 && (cmd)->stub == 0)

typedef struct st_pl2_langauge {
  const char *langName;
  const char *langInfo;

  pl2_Cmd *termCmd;
  
  pl2_InitStub *init;
  pl2_AtexitStub *atExit;
  pl2_SInvokeCmd *sinvokeCmds;
  pl2_PCallCmd *pCallCmds;
  pl2_PCallCmdStub *fallback;
} pl2_Language;

typedef pl2_Language* (pl2_LoadLanguage)(pl2_SemVer version,
                                         pl2_Error *error);
typedef const char **(pl2_EasyLoadLanguage)(void);

/*** ----------------------------- Run ----------------------------- ***/

void pl2_run(pl2_Program *program, pl2_Error *error);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* PLAPI_PL2_H */
