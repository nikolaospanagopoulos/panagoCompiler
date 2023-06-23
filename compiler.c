#include "compiler.h"
#include "compileProcess.h"
#include "stdlib.h"
#include <stdarg.h>
#include <stdio.h>

void compilerError(compileProcess *compiler, const char *msg, ...) {
  va_list args;
  va_start(args, msg);
  vfprintf(stderr, msg, args);
  va_end(args);
  fprintf(stderr, " on line %i, col %i, in file %s\n", compiler->position.line,
          compiler->position.col, compiler->position.filename);
}
void compilerWarning(compileProcess *compiler, const char *msg, ...) {
  va_list args;
  va_start(args, msg);
  vfprintf(stderr, msg, args);
  va_end(args);
  fprintf(stderr, " on line %i, col %i, in file %s\n", compiler->position.line,
          compiler->position.col, compiler->position.filename);
}

struct lexProcessFunctions compilerLexFunctions = {
    .nextChar = compileProcessNextChar,
    .peekChar = compileProcessPeekChar,
    .pushChar = compileProcessPushChar};

int compileFile(const char *filename, const char *outFileName, int flags) {

  compileProcess *process = compileProcessCreate(filename, outFileName, flags);

  if (!process) {
    return COMPILER_FILE_COMPILED_WITH_ERRORS;
  }

  lexProcess *lexProcess =
      lexProcessCreate(process, &compilerLexFunctions, NULL);
  if (!lexProcess) {
    freeCompileProcess(process);
    free(process);

    return COMPILER_FILE_COMPILED_WITH_ERRORS;
  }

  if (lex(lexProcess) != LEX_ALL_OK) {
    freeCompileProcess(process);
    free(process);

    freeLexProcess(lexProcess);
    return LEX_ERROR;
  }
  freeCompileProcess(process);
  free(process);

  freeLexProcess(lexProcess);

  return COMPILER_FILE_COMPILED_OK;
}
