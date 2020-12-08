#include "pl2a.h"
#include <stdio.h>

extern pl2a_Language*
pl2ext_loadLanguage(pl2a_SemVer version, pl2a_Error *error);

static void echo_1_0_0(const char *args[]);

pl2a_Language* pl2ext_loadLanguage(pl2a_SemVer version, pl2a_Error *error) {
  (void)version;
  (void)error;

  static pl2a_SInvokeCmd sinvokeCmds[] = {
    /* cmdName  stub        deprecated  removed*/
    { "echo",   echo_1_0_0, 0,          0    },
    { NULL,     NULL,       NULL,       NULL }
  };

  static pl2a_Language ret = {
    /*langName    = */ "The Echo Language",
    /*langInfo    = */ "This language contains an echo command only",
    /*termCmd     = */ NULL,

    /*init        = */ NULL,
    /*atExit      = */ NULL,
    /*sinvokeCmds = */ sinvokeCmds,
    /*pCallCmds   = */ NULL,
    /*fallback    = */ NULL,
  };
  
  return &ret;
}

static void echo_1_0_0(const char *args[]) {
  for (const char **arg = args + 1; *arg != NULL; ++arg) {
    printf("%s ", *arg);
  }
  putchar('\n');
}
