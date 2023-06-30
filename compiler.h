#pragma once

#include "compileProcess.h"
#include "node.h"

#define S_EQ(str, str2) (str && str2 && (strcmp(str, str2) == 0))
#define SYMBOL_CASE                                                            \
  case '{':                                                                    \
  case '}':                                                                    \
  case ':':                                                                    \
  case ';':                                                                    \
  case '#':                                                                    \
  case '\\':                                                                   \
  case ')':                                                                    \
  case ']'

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
#define OPERATOR_CASE_EXCLUDING_DIVISION                                       \
  case '+':                                                                    \
  case '-':                                                                    \
  case '*':                                                                    \
  case '>':                                                                    \
  case '<':                                                                    \
  case '^':                                                                    \
  case '%':                                                                    \
  case '!':                                                                    \
  case '=':                                                                    \
  case '~':                                                                    \
  case '|':                                                                    \
  case '&':                                                                    \
  case '(':                                                                    \
  case '[':                                                                    \
  case ',':                                                                    \
  case '.':                                                                    \
  case '?'
struct lexProcess;
typedef char (*LEX_PROCESS_NEXT_CHAR)(struct lexProcess *process);
typedef char (*LEX_PROCESS_PEEK_CHAR)(struct lexProcess *process);
typedef void (*LEX_PROCESS_PUSH_CHAR)(struct lexProcess *process, char c);

struct lexProcessFunctions {
  LEX_PROCESS_NEXT_CHAR nextChar;
  LEX_PROCESS_PEEK_CHAR peekChar;
  LEX_PROCESS_PUSH_CHAR pushChar;
};

enum { LEX_ALL_OK, LEX_ERROR };

typedef struct lexProcess {
  pos pos;
  struct vector *tokenVec;
  compileProcess *compiler;
  int currentExpressionCount;
  struct buffer *parenthesesBuffer;
  struct lexProcessFunctions *function;
  void *privateData;

} lexProcess;

enum { PARSE_ALL_OK, PARSE_ERROR };
char compileProcessNextChar(lexProcess *process);
char compileProcessPeekChar(lexProcess *process);
void compileProcessPushChar(lexProcess *process, char c);
int compileFile(const char *filename, const char *outFileName, int flags);
lexProcess *lexProcessCreate(compileProcess *compiler,
                             struct lexProcessFunctions *functions,
                             void *privateData);
void freeLexProcess(lexProcess *process);

void *lexProcessPrivateData(lexProcess *process);
struct vector *lexProcessTokens(lexProcess *process);
int lex(lexProcess *process);
void compilerError(compileProcess *compiler, const char *msg, ...);
void compilerWarning(compileProcess *compiler, const char *msg, ...);
bool tokenIsKeyword(token *token, const char *value);
int parse(compileProcess *process);
node *nodeCreate(node *_node);
node *nodePop();
node *nodePeek();
void nodeSetVector(struct vector *vec, struct vector *rootVec);

void nodePush(node *node);

node *nodePeekOrNull();
