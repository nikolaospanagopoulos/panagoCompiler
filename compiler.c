#include "compiler.h"
#include "compileProcess.h"
#include "stdlib.h"
#include "token.h"
#include "vector.h"
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

  vector_set_peek_pointer(lexProcess->tokenVec, 0);
  struct token *tok = vector_peek(lexProcess->tokenVec);

  while (tok) {
    if (tok->type == NUMBER) {
      printf("NUMBER TOKEN: %llu \n", tok->llnum);
    }

    if (tok->type == STRING) {
      printf("STRING TOKEN: %s\n", tok->sval);
    }
    if (tok->type == OPERATOR) {
      printf("OPERATOR TOKEN: %s\n", tok->sval);
    }
    if (tok->type == SYMBOL) {
      printf("SYMBOL TOKEN: %c\n", tok->cval);
    }
    if (tok->type == IDENTIFIER) {
      printf("IDENTIFIER TOKEN: %s\n", tok->sval);
    }
    if (tok->type == KEYWORD) {
      printf("KEYWORD TOKEN: %s\n", tok->sval);
    }
    if (tok->type == NEWLINE) {
      printf("NEW LINE \n");
    }
    if (tok->type == COMMENT) {
      printf("COMMENT TOKEN: %s\n", tok->sval);
    }
    tok = vector_peek(lexProcess->tokenVec);
  }

  freeCompileProcess(process);
  free(process);

  freeLexProcess(lexProcess);

  return COMPILER_FILE_COMPILED_OK;
}
