#include "buffer.h"
#include "compiler.h"
#include "token.h"
#include "vector.h"
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
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
bool lexInExpression() { return lex_Process->currentExpressionCount > 0; }
static char nextc() {
  char c = lex_Process->function->nextChar(lex_Process);
  if (lexInExpression()) {
    buffer_write(lex_Process->parenthesesBuffer, c);
  }
  lex_Process->pos.col += 1;
  if (c == '\n') {
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
  if (lexInExpression()) {
    tmpToken.betweenBrackets = buffer_ptr(lex_Process->parenthesesBuffer);
  }
  tmpToken.type = _token->type;
  return &tmpToken;
}
int lexerNumType(char c) {
  int res = NORMAL;
  if (c == 'L') {
    res = LONG;
  } else if (c == 'f') {
    res = FLOAT;
  }
  return res;
}
token *tokenMakeNumberForValue(unsigned long number) {
  int numberType = lexerNumType(peekc());
  if (numberType != NORMAL) {
    nextc();
  }
  return tokenCreate(
      &(struct token){.type = NUMBER, .llnum = number, .num.type = numberType});
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
static bool isSingleOperator(char op) {
  return op == '+' || op == '-' || op == '/' || op == '*' || op == '=' ||
         op == '>' || op == '<' || op == '|' || op == '&' || op == '^' ||
         op == '%' || op == '!' || op == '(' || op == '[' || op == ',' ||
         op == '.' || op == '~' || op == '?';
}
static bool opTreatedAsOne(char op) {
  return op == '(' || op == '[' || op == ',' || op == '.' || op == '*' ||
         op == '?';
}
bool opValid(const char *op) {
  return S_EQ(op, "+") || S_EQ(op, "-") || S_EQ(op, "*") || S_EQ(op, "/") ||
         S_EQ(op, "!") || S_EQ(op, "^") || S_EQ(op, "+=") || S_EQ(op, "-=") ||
         S_EQ(op, "*=") || S_EQ(op, "/=") || S_EQ(op, ">>") || S_EQ(op, "<<") ||
         S_EQ(op, ">=") || S_EQ(op, "<=") || S_EQ(op, ">") || S_EQ(op, "<") ||
         S_EQ(op, "||") || S_EQ(op, "&&") || S_EQ(op, "|") || S_EQ(op, "&") ||
         S_EQ(op, "++") || S_EQ(op, "--") || S_EQ(op, "=") || S_EQ(op, "!=") ||
         S_EQ(op, "==") || S_EQ(op, "->") || S_EQ(op, "(") || S_EQ(op, "[") ||
         S_EQ(op, ",") || S_EQ(op, ".") || S_EQ(op, "...") || S_EQ(op, "~") ||
         S_EQ(op, "?") || S_EQ(op, "%");
}
static void pushc(char c) { lex_Process->function->pushChar(lex_Process, c); }
void readOperatorKeepFirstReturnSecond(struct buffer *buffer) {
  const char *data = buffer_ptr(buffer);
  int len = buffer->len;
  for (int i = len - 1; i >= 1; i--) {
    if (data[i] == 0x00) {
      continue;
    }
    pushc(data[i]);
  }
}
struct buffer *readOperator() {
  bool singleOperator = true;
  char op = nextc();
  struct buffer *buffer = buffer_create();
  buffer_write(buffer, op);
  if (!opTreatedAsOne(op)) {
    op = peekc();
    if (isSingleOperator(op)) {
      buffer_write(buffer, op);
      nextc();
      singleOperator = false;
    }
  }
  buffer_write(buffer, 0x00);
  char *ptr = buffer_ptr(buffer);
  if (!singleOperator) {
    if (!opValid(ptr)) {
      readOperatorKeepFirstReturnSecond(buffer);
      ptr[1] = 0x00;
    }
  } else if (!opValid(ptr)) {
    compilerError(lex_Process->compiler, "The operator %s is not valid", ptr);
    errorNum = 1;
  }
  return buffer;
}
static token *lexerLastToken() {
  return vector_back_or_null(lex_Process->tokenVec);
}
static void lexNewExpression() {
  lex_Process->currentExpressionCount++;
  if (lex_Process->currentExpressionCount == 1) {
    lex_Process->parenthesesBuffer = buffer_create();
  }
}

static void lexFinishExpression() {
  lex_Process->currentExpressionCount--;
  buffer_free(lex_Process->parenthesesBuffer);
  if (lex_Process->currentExpressionCount < 0) {
    compilerError(lex_Process->compiler,
                  "You closed an expresssion which you never opened\n");
    errorNum = 1;
  }
}

bool isKeyword(const char *str) {
  return S_EQ(str, "unsigned") || S_EQ(str, "signed") || S_EQ(str, "char") ||
         S_EQ(str, "short") || S_EQ(str, "int") || S_EQ(str, "long") ||
         S_EQ(str, "float") || S_EQ(str, "double") || S_EQ(str, "void") ||
         S_EQ(str, "struct") || S_EQ(str, "union") || S_EQ(str, "static") ||
         S_EQ(str, "__ignore_typecheck") || S_EQ(str, "return") ||
         S_EQ(str, "include") || S_EQ(str, "sizeof") || S_EQ(str, "if") ||
         S_EQ(str, "else") || S_EQ(str, "while") || S_EQ(str, "for") ||
         S_EQ(str, "do") || S_EQ(str, "break") || S_EQ(str, "continue") ||
         S_EQ(str, "switch") || S_EQ(str, "case") || S_EQ(str, "default") ||
         S_EQ(str, "goto") || S_EQ(str, "typedef") || S_EQ(str, "const") ||
         S_EQ(str, "extern") || S_EQ(str, "restrict");
}

token *tokenMakeOperatorOrString() {

  char op = peekc();
  if (op == '<') {
    token *lastToken = lexerLastToken();
    if (tokenIsKeyword(lastToken, "include")) {
      return tokenMakeString('<', '>');
    }
  }

  struct buffer *buffer = readOperator();
  char *stringval = buffer_ptr(buffer);
  char *toCopyToToken = (char *)malloc(strlen(stringval) + 1);
  strncpy(toCopyToToken, stringval, strlen(stringval) + 1);
  token *token =
      tokenCreate(&(struct token){.type = OPERATOR, .sval = toCopyToToken});
  if (op == '(') {
    lexNewExpression();
  }
  buffer_free(buffer);

  return token;
}
bool isValid(const char *str) {
  size_t len = strlen(str);
  for (int i = 0; i < len; i++) {
    if (str[i] != '1' && str[i] != '0') {
      return false;
    }
  }
  return true;
}
token *makeBinary() {
  nextc(); // skip b
  unsigned long number = 0;
  struct buffer *buffer = readNumberStr();
  if (isValid(buffer_ptr(buffer))) {
    number = strtol(buffer_ptr(buffer), 0, 2);
    buffer_free(buffer);
    return tokenMakeNumberForValue(number);
  }
  buffer_free(buffer);
  compilerError(lex_Process->compiler, "Not a valid binary number. \n");
  errorNum = 1;
  return NULL;
}
static struct token *tokenMakeSymbol() {
  char c = nextc();
  if (c == ')') {
    lexFinishExpression();
  }
  token *token = tokenCreate(&(struct token){.type = SYMBOL, .cval = c});
  return token;
}
static token *tokenMakeIdentifierOrKeyword() {

  struct buffer *buffer = buffer_create();
  char c = 0;
  LEX_GETC_IF(buffer, c,
              (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '_');
  buffer_write(buffer, 0x00);
  char *stringval = buffer_ptr(buffer);
  char *toCopyToToken = (char *)malloc(strlen(stringval) + 1);
  strncpy(toCopyToToken, stringval, strlen(stringval) + 1);

  token *token = NULL;

  if (isKeyword(buffer_ptr(buffer))) {
    token =
        tokenCreate(&(struct token){.type = KEYWORD, .sval = toCopyToToken});

  } else {
    token =
        tokenCreate(&(struct token){.type = IDENTIFIER, .sval = toCopyToToken});
  }

  buffer_free(buffer);
  return token;
}
token *readSpecialToken() {
  char c = peekc();
  if (isalpha(c) || c == '_') {
    return tokenMakeIdentifierOrKeyword();
  }
  return NULL;
}

token *tokenMakeNewLine() {
  nextc();
  return tokenCreate(&(struct token){.type = NEWLINE});
}

token *tokenMakeOneLineComment() {
  struct buffer *buffer = buffer_create();
  char c = 0;
  LEX_GETC_IF(buffer, c, c != '\n' && c != EOF);
  buffer_write(buffer, 0x00);
  char *stringval = buffer_ptr(buffer);
  char *toCopyToToken = (char *)malloc(strlen(stringval) + 1);
  strncpy(toCopyToToken, stringval, strlen(stringval) + 1);
  buffer_free(buffer);
  return tokenCreate(&(struct token){.type = COMMENT, .sval = toCopyToToken});
}

token *tokenMakeMultilineComment() {
  struct buffer *buffer = buffer_create();
  char c = 0;
  while (1) {
    LEX_GETC_IF(buffer, c, c != '*' && c != EOF);
    if (c == EOF) {
      compilerError(lex_Process->compiler,
                    "Multiline comment wasn't ended properly \n");
      errorNum = 1;
      break;
    } else if (c == '*') {
      nextc();
      if (peekc() == '/') {
        nextc();
        break;
      }
    }
  }
  char *stringval = buffer_ptr(buffer);
  char *toCopyToToken = (char *)malloc(strlen(stringval) + 1);
  strncpy(toCopyToToken, stringval, strlen(stringval) + 1);
  buffer_free(buffer);
  return tokenCreate(&(struct token){.type = COMMENT, .sval = toCopyToToken});
}

token *handleComment() {
  char c = peekc();
  if (c == '/') {
    nextc();
    if (peekc() == '/') {
      nextc();
      return tokenMakeOneLineComment();
    } else if (peekc() == '*') {
      nextc();
      return tokenMakeMultilineComment();
    }
    // division
    pushc('/');
    return tokenMakeOperatorOrString();
  }
  return NULL;
}
char lexGetEscapedChar(char c) {
  char co = 0;
  switch (c) {
  case 'n':
    co = '\n';
    break;
  case '\\':
    co = '\\';
    break;
  case 't':
    co = '\t';
    break;
  case '\'':
    co = '\'';
    break;
  }
  return co;
}
static char assertNextChar(char c) {
  char nextChar = nextc();
  if (nextChar != c) {
    compilerError(lex_Process->compiler, "Not an expected char after quote \n");
    errorNum = 1;
  }
  return c;
}
void lexerPopToken() { vector_pop(lex_Process->tokenVec); }

bool isHexChar(char c) {
  c = tolower(c);
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
}

struct buffer *readHexNumberStr() {
  struct buffer *buffer = buffer_create();
  char c = peekc();
  LEX_GETC_IF(buffer, c, isHexChar(c));

  buffer_write(buffer, 0x00);

  return buffer;
}

token *tokenMakeHex() {
  // skip x
  nextc();
  unsigned long number = 0;
  struct buffer *numberStr = readHexNumberStr();
  number = strtol(buffer_ptr(numberStr), 0, 16);
  buffer_free(numberStr);
  return tokenMakeNumberForValue(number);
}

token *makeSpecialNum() {
  token *token = NULL;
  struct token *lastToken = lexerLastToken();
  // remove 0 . ex 0x555
  if (!lastToken || !(lastToken->type == NUMBER && lastToken->llnum == 0)) {
    return tokenMakeIdentifierOrKeyword();
  }
  lexerPopToken();
  char c = peekc();
  if (c == 'x') {
    token = tokenMakeHex();
  } else if (c == 'b') {
    token = makeBinary();
  }
  return token;
}
token *tokenMakeQuote() {

  assertNextChar('\'');
  char c = nextc();
  if (c == '\\') {
    // get escaped char
    c = nextc();
    c = lexGetEscapedChar(c);
  }
  if (nextc() != '\'') {
    compilerError(lex_Process->compiler,
                  "A quote was opened but was never closed");
  }
  return tokenCreate(&(token){.type = NUMBER, .cval = c});
}

token *readNextToken() {
  token *token = NULL;
  char c = peekc();

  token = handleComment();
  if (token) {
    return token;
  }

  switch (c) {
  NUMERIC_CASE:
    token = tokenMakeNumber();
    break;
  OPERATOR_CASE_EXCLUDING_DIVISION:
    token = tokenMakeOperatorOrString();
    break;
  SYMBOL_CASE:
    token = tokenMakeSymbol();
    break;
  case 'b':
  case 'x':
    token = makeSpecialNum();
    break;
  case '"':
    token = tokenMakeString('"', '"');
    break;
  case '\'':
    token = tokenMakeQuote();
    break;
  case ' ':
  case '\t':
    token = handleWhitespace();
    break;
  case EOF:
    break;
  case '\n':
    token = tokenMakeNewLine();
    break;
  default:
    token = readSpecialToken();
    if (!token) {
      compilerError(lex_Process->compiler, "Unexpected token \n");
      errorNum = 1;
    }
  }
  return token;
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
  bool encounteredError = false;

  process->currentExpressionCount = 0;
  process->parenthesesBuffer = NULL;
  lex_Process = process;
  process->pos.filename = process->compiler->cfile.absolutePath;

  token *token = readNextToken();
  while (token) {
    vector_push(process->tokenVec, token);
    token = readNextToken();
  }

  if (errorNum != 0) {
    encounteredError = true;
    return LEX_ERROR;
  }
  vector_set_peek_pointer(lex_Process->tokenVec, 0);
  struct token *tok = vector_peek(lex_Process->tokenVec);

  return LEX_ALL_OK;
}
