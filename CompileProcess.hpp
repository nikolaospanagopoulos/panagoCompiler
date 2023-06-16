#pragma once
#include "Position.h"
#include <cstdio>
#include <sstream>
#include <string>

#define NUMERIC_CASE                                                           \
  case '0':                                                                    \
  case '1':                                                                    \
  case '2':                                                                    \
  case '3':                                                                    \
  case '4':                                                                    \
  case '5':                                                                    \
  case '6':                                                                    \
  case '7':                                                                    \
  case '8':                                                                    \
  case '9'

struct compileProccessInputFile {
  FILE *inputFilePtr;
  const char *absolutePath;
  compileProccessInputFile();
};
class CompileProcess {
public:
  int flags;
  compileProccessInputFile cfile;
  FILE *outFile;
  pos position;
  template <typename... Args>
  static std::string compilerError(const CompileProcess *compiler,
                                   std::string message, Args &&...args);
  CompileProcess();
  ~CompileProcess();
};

template <typename... Args>
std::string CompileProcess::compilerError(const CompileProcess *compiler,
                                          std::string message, Args &&...args) {
  std::stringstream errorStream;
  errorStream << "Error: ";
  errorStream << message;
  errorStream << " on line " << compiler->position.line << " in column "
              << compiler->position.col << " in file "
              << compiler->position.filename << "\n";
  int unused[] = {((errorStream << args))...};
  return errorStream.str();
}
