#include "pl2a.h"
#include <stdio.h>

extern pl2a_Language*
pl2ext_loadLanguage(pl2a_SemVer version, pl2a_Error *error);

static pl2a_Cmd*
pldbg_fallback(pl2a_Program *program,
               void *context,
               pl2a_Cmd *cmd,
               pl2a_Error *error);

pl2a_Language *pl2ext_loadLanguage(pl2a_SemVer version, pl2a_Error *error) {
  (void)version;
  (void)error;
  
  static pl2a_Cmd termCmd;
  
  static pl2a_Language ret = {
    /*langName    = */ "PL2 external debugger",
    /*langInfo    = */ "this language will display intaking commands",
    /*termCmd     = */ &termCmd,

    /*init        = */ NULL,
    /*atExit      = */ NULL,
    /*sinvokeCmds = */ NULL,
    /*pCallCmds   = */ NULL,
    /*fallback    = */ pldbg_fallback
  };
  
  return &ret;
}

static pl2a_Cmd *pldbg_fallback(pl2a_Program *program,
                                void *context,
                                pl2a_Cmd *cmd,
                                pl2a_Error *error) {
  (void)program;
  (void)context;
  (void)error;

  fprintf(stderr, "%5u|  ", cmd->sourceInfo.line);
  fprintf(stderr, "%s\t", cmd->cmd);
  for (char **arg = cmd->args; *arg != NULL; arg++) {
    fprintf(stderr, "\"%s\" ", *arg);
  }
  fputc('\n', stderr);
  return NULL;
}
