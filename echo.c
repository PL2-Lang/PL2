#include "pl2.h"
#include <stdio.h>

extern pl2_Language*
pl2ext_loadLanguage(pl2_SemVer version, pl2_Error *error);

static void echo_1_0_0(const char *args[]);

pl2_Language* pl2ext_loadLanguage(pl2_SemVer version, pl2_Error *error) {
  (void)version;
  (void)error;

  static pl2_SInvokeCmd sinvokeCmds[] = {
    { "echo", echo_1_0_0, 0, 0}
  };

  static pl2_Language ret = {
    "The Echo Language",
    "This language contains a simple command echo only",
    NULL,

    NULL,
    NULL,
    sinvokeCmds,
    NULL,
    NULL,
  };
  
  return &ret;
  
}

static void echo_1_0_0(const char *args[]) {
  for (const char **arg = args + 1; *arg != NULL; ++arg) {
    printf("%s ", *arg);
  }
  putchar('\n');
}
