#include "pl2.h"
#include <stdio.h>

extern pl2_Language*
pl2ext_loadLanguage(pl2_SemVer version, pl2_Error *error);

static pl2_Cmd*
pl2ext_pldbg_fallback(pl2_Program *program,
                     void *context,
                     pl2_Cmd *cmd,
                     pl2_Error *error);

pl2_Language *pl2ext_loadLanguage(pl2_SemVer version, pl2_Error *error) {
  (void)version;
  (void)error;
  
  static pl2_Cmd termCmd;
  
  static pl2_Language ret = {
    "PL2 external debugger",
    "this language will display intaking commands",
    &termCmd,

    NULL,
    NULL,
    NULL,
    NULL,
    pl2ext_pldbg_fallback
  };
  
  return &ret;
}

static pl2_Cmd *pl2ext_pldbg_fallback(pl2_Program *program,
                                      void *context,
                                      pl2_Cmd *cmd,
                                      pl2_Error *error) {
  (void)program;
  (void)context;
  (void)error;

  fprintf(stderr, "%5u|  ", cmd->line);
  for (pl2_CmdPart *part = cmd->parts; !PL2_EMPTY_PART(part); part++) {
    if (!pl2_isNullSlice(part->prefix)) {
      fprintf(
        stderr,
        "%s\"%s\" ",
        pl2_unsafeIntoCStr(part->prefix),
        pl2_unsafeIntoCStr(part->body)
      );
    } else {
      fprintf(stderr, "\"%s\" ", pl2_unsafeIntoCStr(part->body));
    }
  }
  fputc('\n', stderr);
  return NULL;
}
