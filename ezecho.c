#include <stdio.h>

const char **pl2ezload(void) {
  static const char *ret[] = {
    "echo", NULL
  };
  return ret;
}

void pl2ez_echo(const char **args) {
  for (; *args != NULL; ++args) {
    printf("%s ", *args);
  }
  putchar('\n');
}
