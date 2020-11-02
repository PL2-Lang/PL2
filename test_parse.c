#include "pl2.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *source = 
"# comment line\n"
"language txt_lang 1.0\n"
"echo \"Hello \\\"world\\\"\" prefix\"body\"\n"
"?begin multi\n"
"line command \"and string\"\n"
"?end tail\n";

int main() {
  pl2_Error *error = pl2_errorBuffer(512);
  char *sourceBuffer = (char*)malloc(strlen(source) + 1);
  strcpy(sourceBuffer, source);
  
  pl2_Program program = pl2_parse(sourceBuffer, error);
  if (pl2_isError(error)) {
    fprintf(stderr, "error: %s\n", error->reason);
    return -1;
  }
  
  pl2_debugPrintProgram(&program);
  pl2_clearProgram(&program);
  free(error);
  free(sourceBuffer);
  return 0;
}
