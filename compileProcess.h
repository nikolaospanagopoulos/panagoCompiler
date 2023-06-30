#pragma once
#include "stdio.h"
#include "token.h"

enum { COMPILER_FILE_COMPILED_OK, COMPILER_FILE_COMPILED_WITH_ERRORS };

typedef struct compileProcess {
  int flags;
  pos position;
  struct compileProcessInputFile {
    FILE *fp;
    const char *absolutePath;
  } cfile;
  FILE *outFile;
  struct vector *tokenVec;
  struct vector *nodeVec;
  struct vector *nodeTreeVec;
} compileProcess;

void freeCompileProcess(compileProcess *cp);
compileProcess *compileProcessCreate(const char *filename,
                                     const char *filenameOutName, int flags);
