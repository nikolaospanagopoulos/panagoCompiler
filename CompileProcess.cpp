#include "CompileProcess.hpp"
#include "Position.h"
#include <cstdio>
#include <iostream>

compileProccessInputFile::compileProccessInputFile()
    : inputFilePtr(nullptr), absolutePath("") {}

CompileProcess::CompileProcess()
    : flags(0), cfile{}, outFile{nullptr}, position() {
  std::cout << "compile process initialized \n";
}
CompileProcess::~CompileProcess() {
  if (outFile) {
    fclose(outFile);
  }
  if (cfile.inputFilePtr) {
    fclose(cfile.inputFilePtr);
  }

  std::cout << "Compile process destructor called \n";
}
