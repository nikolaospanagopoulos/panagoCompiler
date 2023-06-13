#pragma once
#include "Position.h"
#include <cstdio>
struct compileProccessInputFile {
  FILE *inputFilePtr;
  const char *absolutePath;
  compileProccessInputFile();
};
struct CompileProcess {
  int flags;
  compileProccessInputFile cfile;
  FILE *outFile;
  pos position;
  CompileProcess();
  ~CompileProcess();
};
