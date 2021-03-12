#include "pl2b.h"
#include <stdio.h>

extern pl2b_Language*
pl2ext_loadLanguage(pl2b_SemVer version, pl2b_Error *error);

static pl2b_Cmd*
pldbg_fallback(pl2b_Program *program,
               void *context,
               pl2b_Cmd *cmd,
               pl2b_Error *error);

pl2b_Language *pl2ext_loadLanguage(pl2b_SemVer version, pl2b_Error *error) {
  (void)version;
  (void)error;

  static pl2b_Cmd termCmd;

  static pl2b_Language ret = {
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

static pl2b_Cmd *pldbg_fallback(pl2b_Program *program,
                                void *context,
                                pl2b_Cmd *cmd,
                                pl2b_Error *error) {
  (void)program;
  (void)context;
  (void)error;

  fprintf(stderr, "%5u|  ", cmd->sourceInfo.line);
  if (cmd->cmd.isString) {
    fprintf(stderr, "\"%s\"\t", cmd->cmd.str);
  } else {
    fprintf(stderr, "%s\t", cmd->cmd.str);
  }

  for (pl2b_CmdPart *arg = cmd->args; PL2B_EMPTY_PART(*arg); arg++) {
    if (arg->isString) {
      fprintf(stderr, "\"%s\"\t", arg->str);
    } else {
      fprintf(stderr, "%s\t", arg->str);
    }
  }
  fputc('\n', stderr);
  return NULL;
}
