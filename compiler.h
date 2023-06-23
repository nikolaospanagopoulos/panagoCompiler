#pragma once

#include "compileProcess.h"

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
