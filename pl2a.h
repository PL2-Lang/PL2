#ifndef PLAPI_PL2A_H
#define PLAPI_PL2A_H

#define PL2A_API

/*** -------------------------- versioning ------------------------- ***/

#define PL2_EDITION       "PL2-A"    /* Latin name */
#define PL2_EDITION_CN    "霹雳2-乙" /* Chinese name */
#define PL2_EDITION_RU    "ПЛ2-А"    /* Cyrillic name */
#define PL2_EDITION_JP    "ピリ-2"   /* Japanese name */
#define PL2_EDITION_KR    "펠리-2"   /* Kroean name */
#define PL2A_VER_MAJOR    0          /* Major version of PL2A */
#define PL2A_VER_MINOR    1          /* Minor version of PL2A */
#define PL2A_VER_PATCH    1          /* Patch version of PL2A */
#define PL2A_VER_POSTFIX  "halley"   /* Version postfix */

const char *pl2a_getLocaleName(void);

/*** ---------------------- end configurations --------------------- ***/

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*** ------------------------ pl2a_CmpResult ----------------------- ***/

typedef enum e_pl2a_cmp_result {
  PL2A_CMP_LESS    = -1, /* LHS < RHS */
  PL2A_CMP_EQ      = 0,  /* LHS = RHS */
  PL2A_CMP_GREATER = 1,  /* LHS > RHS */
  PL2A_CMP_NONE    = 255 /* not comparable */
} pl2a_CmpResult;

/*** ----------------------- pl2a_SourceInfo  ---------------------- ***/

typedef struct st_pl2a_source_info {
    const char *fileName;
    uint16_t line;
} pl2a_SourceInfo;

pl2a_SourceInfo pl2a_sourceInfo(const char *fileName, uint16_t line);

/*** -------------------------- pl2a_Error ------------------------- ***/

typedef struct st_pl2a_error {
  void *extraData;
  pl2a_SourceInfo sourceInfo;
  uint16_t errorCode;
  uint16_t errorBufferSize;
  char reason[0];
} pl2a_Error;

typedef enum e_pl2a_error_code {
  PL2A_ERR_NONE           = 0,  /* no error */
  PL2A_ERR_GENERAL        = 1,  /* general hard error */
  PL2A_ERR_PARSEBUF       = 2,  /* parse buffer exceeded */
  PL2A_ERR_UNCLOSED_STR   = 3,  /* unclosed string literal */
  PL2A_ERR_UNCLOSED_BEGIN = 4,  /* unclosed ?begin block */
  PL2A_ERR_EMPTY_CMD      = 5,  /* empty command */
  PL2A_ERR_SEMVER_PARSE   = 6,  /* semver parse error */
  PL2A_ERR_UNKNOWN_QUES   = 7,  /* unknown question mark command */
  PL2A_ERR_LOAD_LANG      = 8,  /* error loading language */
  PL2A_ERR_NO_LANG        = 9,  /* language not loaded */
  PL2A_ERR_UNKNOWN_CMD    = 10, /* unknown command */
  PL2A_ERR_MALLOC         = 11, /* malloc failure*/

  PL2A_ERR_USER           = 100 /* generic user error */
} pl2a_ErrorCode;

pl2a_Error *pl2a_errorBuffer(uint16_t strBufferSize);

void pl2a_errPrintf(pl2a_Error *error,
                    uint16_t errorCode,
                    pl2a_SourceInfo sourceInfo,
                    void *extraData,
                    const char *fmt,
                    ...);

void pl2a_dropError(pl2a_Error *error);

_Bool pl2a_isError(pl2a_Error *error);

/*** --------------------------- pl2a_Cmd -------------------------- ***/

typedef struct st_pl2a_cmd {
  struct st_pl2a_cmd *prev;
  struct st_pl2a_cmd *next;

  void *extraData;
  pl2a_SourceInfo sourceInfo;
  char *cmd;
  char *args[0];
} pl2a_Cmd;

pl2a_Cmd *pl2a_cmd3(pl2a_SourceInfo sourceInfo,
                    char *cmd,
                    char *args[]);

pl2a_Cmd *pl2a_cmd6(pl2a_Cmd *prev,
                    pl2a_Cmd *next,
                    void *extraData,
                    pl2a_SourceInfo sourceInfo,
                    char *cmd,
                    char *args[]);

uint16_t pl2a_argsLen(pl2a_Cmd *cmd);

/*** ------------------------- pl2a_Program ------------------------ ***/

typedef struct st_pl2a_program {
  pl2a_Cmd *commands;
} pl2a_Program;

void pl2a_initProgram(pl2a_Program *program);
pl2a_Program pl2a_parse(char *source,
                        uint16_t parseBufferSize,
                        pl2a_Error *error);
void pl2a_dropProgram(pl2a_Program *program);
void pl2a_debugPrintProgram(const pl2a_Program *program);

/*** -------------------- Semantic-ver parsing  -------------------- ***/

#define PL2A_SEMVER_POSTFIX_LEN 15

typedef struct st_pl2a_sem_ver {
  uint16_t major;
  uint16_t minor;
  uint16_t patch;
  char postfix[PL2A_SEMVER_POSTFIX_LEN];
  _Bool exact;
} pl2a_SemVer;

pl2a_SemVer pl2a_zeroVersion(void);
pl2a_SemVer pl2a_parseSemVer(const char *src, pl2a_Error *error);
_Bool pl2a_isZeroVersion(pl2a_SemVer ver);
_Bool pl2a_isAlpha(pl2a_SemVer ver);
_Bool pl2a_isStable(pl2a_SemVer ver);
_Bool pl2a_isCompatible(pl2a_SemVer expected, pl2a_SemVer actual);
pl2a_CmpResult pl2a_semverCmp(pl2a_SemVer ver1, pl2a_SemVer ver2);
void pl2a_semverToString(pl2a_SemVer ver, char *buffer);

/*** ------------------------ pl2a_Extension ----------------------- ***/

typedef void (pl2a_SInvokeCmdStub)(const char *strings[]);
typedef pl2a_Cmd *(pl2a_PCallCmdStub)(pl2a_Program *program,
                                      void *context,
                                      pl2a_Cmd *command,
                                      pl2a_Error *error);
typedef _Bool (pl2a_CmdRouterStub)(const char *str);

typedef void *(pl2a_InitStub)(pl2a_Error *error);
typedef void (pl2a_AtexitStub)(void *context);

typedef struct st_pl2a_sinvoke_cmd {
  const char *cmdName;
  pl2a_SInvokeCmdStub *stub;
  _Bool deprecated;
  _Bool removed;
} pl2a_SInvokeCmd;

typedef struct st_pl2a_pcall_func {
  const char *cmdName;
  pl2a_CmdRouterStub *routerStub;
  pl2a_PCallCmdStub *stub;
  _Bool deprecated;
  _Bool removed;
} pl2a_PCallCmd;

#define PL2A_EMPTY_SINVOKE_CMD(cmd) \
  ((cmd)->cmdName == 0 && (cmd)->stub == 0)
#define PL2A_EMPTY_CMD(cmd) \
  ((cmd)->cmdName == 0 && (cmd)->routerStub == 0 && (cmd)->stub == 0)

typedef struct st_pl2a_langauge {
  const char *langName;
  const char *langInfo;

  pl2a_Cmd *termCmd;

  pl2a_InitStub *init;
  pl2a_AtexitStub *atExit;
  pl2a_SInvokeCmd *sinvokeCmds;
  pl2a_PCallCmd *pCallCmds;
  pl2a_PCallCmdStub *fallback;
} pl2a_Language;

typedef pl2a_Language *(pl2a_LoadLanguage)(pl2a_SemVer version,
                                           pl2a_Error *error);
typedef const char **(pl2a_EasyLoadLanguage)(void);

/*** ----------------------------- Run ----------------------------- ***/

void pl2a_run(pl2a_Program *program, pl2a_Error *error);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* PLAPI_PL2A_H */
