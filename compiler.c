#include "compiler.h"
#include "compileProcess.h"
#include "node.h"
#include "stdlib.h"
#include "token.h"
#include "vector.h"
#include <stdarg.h>
#include <stdio.h>
static struct lexProcess *lrxPr;
static void setLexProcessForCompileProcess(struct lexProcess *pr) {
  lrxPr = pr;
}
void compilerError(compileProcess *compiler, const char *msg, ...) {
  va_list args;
  va_start(args, msg);
  vfprintf(stderr, msg, args);
  va_end(args);
  fprintf(stderr, " on line %i, col %i, in file %s\n", compiler->position.line,
          compiler->position.col, compiler->position.filename);
  freeLexProcess(lrxPr);
  freeCompileProcess(compiler);
  free(compiler);
  exit(-1);
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

  setLexProcessForCompileProcess(lexProcess);
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

  // maybe check when to clean vector
  process->tokenVec = lexProcess->tokenVec;
  setCompileProcessLexProcess(process, lexProcess);
  setLexProcessCompileProcess(lexProcess, process);

  if (parse(process) != PARSE_ALL_OK) {

    freeLexProcess(lexProcess);
    freeCompileProcess(process);
    free(process);
    return PARSE_ERROR;
  }
  setCompileProcessForStackFrame(process);
  setCompileProcessForResolverDefaultHandler(process);

  if (codegen(process) != CODEGEN_ALL_OK) {
    freeLexProcess(lexProcess);
    freeCompileProcess(process);
    free(process);
  }

  freeLexProcess(lexProcess);
  freeCompileProcess(process);
  free(process);

  return COMPILER_FILE_COMPILED_OK;
}
