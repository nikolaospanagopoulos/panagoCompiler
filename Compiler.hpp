#pragma once

#include "CompileProcess.hpp"
#include "Lexer.hpp"
#include "vector.h"

enum { LEXICAL_ANALYSIS_ALL_OK, LEXICAL_ANALYSIS_ERROR };

class Compiler {

public:
  int compileFile(const char *filename, const char *outFilename, int flags);
  CompileProcess *compileProcess;
  LexProcess *lexProcess;
  struct CompileProcess *compileProcessCreate(const char *filename,
                                              const char *filenameOut,
                                              int flags);

  int lex();
  Compiler();
  ~Compiler();
};
