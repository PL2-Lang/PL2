#include "pl2.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, const char *argv[]) {
  fprintf(stderr, 
    "PL2 programming language platform\n"
    "  Author:  ICEY<icey@icey.tech>\n"
    "  Version: v%u.%u.%u %s\n"
    "  License: AGPLv3\n\n",
    PL2_VER_MAJOR,
    PL2_VER_MINOR,
    PL2_VER_PATCH,
    PL2_VER_POSTFIX);
  
  if (argc != 2) {
    fprintf(stderr, "expected 1 argument, got %d\n", argc - 1);
    return -1;
  }
  
  FILE *fp = fopen(argv[1], "r");
  if (fp == NULL) {
    fprintf(stderr, "cannot open input file %s\n", argv[1]);
    return -1;
  }
  
  long fileSize = 0;
  if (fseek(fp, 0, SEEK_END) < 0) {
    fprintf(stderr, "cannot determine file size\n");
    return -1;
  }
  fileSize = ftell(fp);
  if (fileSize < 0) {
    fprintf(stderr, "cannot determine file size\n");
    return -1;
  }
  
  char *buffer = (char*)malloc((size_t)fileSize + 1);
  memset(buffer, 0, (size_t)fileSize + 1);
  rewind(fp);
  if ((long)fread(buffer, 1, (size_t)fileSize, fp) < fileSize) {
    fprintf(stderr, "cannot read file\n");
    return -1;
  }
  
  pl2_Error *error = pl2_errorBuffer(512);
  pl2_Program program = pl2_parse(buffer, error);
  if (pl2_isError(error)) {
    fprintf(stderr, 
            "parsing error %d: line %d: %s\n",
            error->errorCode,
            error->line,
            error->reason);
    return -1;
  }
  
  int ret = 0;
  pl2_run(&program, error);
  if (pl2_isError(error)) {
    fprintf(stderr, 
            "runtime error %d: line %d: %s\n",
            error->errorCode,
            error->line,
            error->reason);
    ret = -1;
  }
  
  pl2_dropProgram(&program);
  pl2_dropError(error);
  free(buffer);
  fclose(fp);
  
  return ret;
}

