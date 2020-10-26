#ifndef PLAPI_PL2_H
#define PLAPI_PL2_H

#define PLAPI(X)

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t pl2_MString;
typedef struct st_m_context {
} pl2_MContext;

pl2_MContext *pl2_mContext();
void pl2_mContextFree(pl2_MContext *context);
pl2_MString pl2_mString(pl2_MContext *context, const char *string);
const char *pl2_getString(pl2_MContext *context, pl2_MString mstring);

typedef struct st_pl2_source_info {
  const char *fileName;
  uint16_t line;
} pl2_SourceInfo;

typedef struct st_pl2_command_part {
  const char *prefix;
  const char *body;
} pl2_CommandPart;

#define PL2_EMPTY_PART(part) ((part)->body == 0 && (part)->prefix == 0)

typedef struct st_pl2_command {
  struct st_pl2_command *prev;
  struct st_pl2_command *next;

  void *extraData;
  pl2_CommandPart parts[0];
} pl2_Command;

typedef struct st_pl2_program {
  const char *language;
  const char *libName;
  pl2_Command *commands;

  void *extraData;
} pl2_Program;

typedef void (pl2_SInvokeFuncStub)(const char *strings[], uint16_t n);
typedef pl2_Command* (pl2_PCallFuncStub)(pl2_Program *program,
                                         void *context,
                                         pl2_Command *command);

typedef struct st_pl2_sinvoke_func {
  pl2_MString funcName;
  pl2_SInvokeFuncStub *stub;
} pl2_SInvokeFunc;

typedef struct st_pl2_pcall_func {
  pl2_MString funcName;
  pl2_PCallFuncStub *stub;
} pl2_PCallFunc;

#define PL2_EMPTY_FUNC(func) ((func)->funcName == 0 && (func)->stub == 0)

typedef struct st_pl2_context {
  pl2_Program program;
  pl2_MContext *mContext;

  pl2_Command *currentCommand;
  pl2_SInvokeFunc *sinvokeFuncs;
  pl2_PCallFunc *pcallFuncs;
  pl2_PCallFuncStub *fallback;
  void *context;
} pl2_Context;

pl2_Program pl2_parse(char *source);
pl2_Context pl2_loadLanguage(pl2_Program program);
void pl2_run(pl2_Context *program);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* PLAPI_PL2_H */
