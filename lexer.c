#include "compiler.h"
#include "token.h"
#include "vector.h"
#include <stdio.h>
static struct lexProcess *lex_Process;

static char peekc() { return lex_Process->function->peekChar(lex_Process); }
static void pushc(char c) { lex_Process->function->pushChar(lex_Process, c); }
token *readNextToken() {
  token *token = NULL;
  char c = peekc();
  switch (c) {
  case EOF:
    break;
  default:
    compilerError(lex_Process->compiler, "Unexpected token \n");
  }
  return token;
}

int lex(lexProcess *process) {

  process->currentExpressionCount = 0;
  process->parenthesesBuffer = NULL;
  lex_Process = process;
  process->pos.filename = process->compiler->cfile.absolutePath;

  token *token = readNextToken();
  while (true) {
    if (token == NULL) {
      return LEX_ERROR;
    }
    vector_push(process->tokenVec, token);
    token = readNextToken();
  }

  return LEX_ALL_OK;
}
