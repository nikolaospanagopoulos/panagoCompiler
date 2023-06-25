#include "buffer.h"
#include "compiler.h"
#include "token.h"
#include "vector.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define LEX_GETC_IF(buffer, c, exp)                                            \
  for (c = peekc(); exp; c = peekc()) {                                        \
    buffer_write(buffer, c);                                                   \
    nextc();                                                                   \
  }
static int errorNum = 0;
static struct lexProcess *lex_Process;
static token tmpToken;
static char nextc() {
  char c = lex_Process->function->nextChar(lex_Process);
  lex_Process->pos.col += 1;
  if (c != '\n') {
    lex_Process->pos.line += 1;
    lex_Process->pos.col = 1;
  }
  return c;
}
static char peekc() { return lex_Process->function->peekChar(lex_Process); }
struct buffer *readNumberStr() {
  struct buffer *buffer = buffer_create();
  char c = peekc();
  LEX_GETC_IF(buffer, c, (c >= '0' && c <= '9'));
  buffer_write(buffer, 0x00);
  return (buffer);
}
static pos lexFilePosition() { return lex_Process->pos; }
unsigned long long readNumber() {
  struct buffer *buffer = readNumberStr();

  long long num = atoll(buffer_ptr(buffer));
  buffer_free(buffer);
  return num;
}
token *tokenCreate(token *_token) {
  memcpy(&tmpToken, _token, sizeof(token));
  tmpToken.position = lexFilePosition();
  tmpToken.type = _token->type;
  return &tmpToken;
}
token *tokenMakeNumberForValue(unsigned long number) {
  return tokenCreate(&(struct token){.type = NUMBER, .llnum = number});
}

token *tokenMakeNumber() { return tokenMakeNumberForValue(readNumber()); }

static token *tokenMakeString(char startDelim, char endDelim) {
  struct buffer *buffer = buffer_create();
  nextc();
  char c = nextc();
  for (; c != endDelim && c != EOF; c = nextc()) {
    if (c == '\\') {
      continue;
    }
    buffer_write(buffer, c);
  }
  buffer_write(buffer, 0x00);

  char *stringval = buffer_ptr(buffer);
  char *toCopyToToken = (char *)malloc(strlen(stringval) + 1);
  strncpy(toCopyToToken, stringval, strlen(stringval) + 1);

  token *tok = tokenCreate(&(token){.type = STRING, .sval = toCopyToToken});
  buffer_free(buffer);
  return tok;
}

static void pushc(char c) { lex_Process->function->pushChar(lex_Process, c); }
token *readNextToken() {
  token *token = NULL;
  char c = peekc();
  switch (c) {
  NUMERIC_CASE:
    token = tokenMakeNumber();
    break;
  case '"':
    token = tokenMakeString('"', '"');
    break;
  case ' ':
  case '\t':
    token = handleWhitespace();
    break;
  case EOF:
  case '\n':
    break;
  default:
    compilerError(lex_Process->compiler, "Unexpected token \n");
    errorNum = 1;
  }
  return token;
}
static token *lexerLastToken() {
  return vector_back_or_null(lex_Process->tokenVec);
}
static token *handleWhitespace() {
  token *last = lexerLastToken();
  if (last) {
    last->whitespace = true;
  }
  nextc();
  return readNextToken();
}
int lex(lexProcess *process) {

  process->currentExpressionCount = 0;
  process->parenthesesBuffer = NULL;
  lex_Process = process;
  process->pos.filename = process->compiler->cfile.absolutePath;

  token *token = readNextToken();
  while (token) {
    if (errorNum != 0) {
      return LEX_ERROR;
    }
    vector_push(process->tokenVec, token);
    token = readNextToken();
  }

  vector_set_peek_pointer(lex_Process->tokenVec, 0);
  struct token *tok = vector_peek(lex_Process->tokenVec);

  return LEX_ALL_OK;
}
