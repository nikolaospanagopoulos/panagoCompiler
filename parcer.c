#include "compileProcess.h"
#include "compiler.h"
#include "node.h"
#include "position.h"
#include "token.h"
#include "vector.h"
#include <stdbool.h>
#include <stdio.h>
int errorHappened = 0;
static struct compileProcess *currentProcess;
static token *parserLastToken;

bool tokenIsSymbol(token *token, char c) {
  return token->type == SYMBOL && token->cval == c;
}

static bool tokenIsNewLineOrComment(token *token) {
  return token->type == NEWLINE || token->type == COMMENT ||
         tokenIsSymbol(token, '\\');
}
static void parserIgnoreNlOrComment(token *token) {
  while (token && tokenIsNewLineOrComment(token)) {
    // skip
    vector_peek(currentProcess->tokenVec);
    token = vector_peek_no_increment(currentProcess->tokenVec);
  }
}

static token *tokenNext() {
  token *nextToken = vector_peek_no_increment(currentProcess->tokenVec);
  parserIgnoreNlOrComment(nextToken);
  currentProcess->position = nextToken->position;
  parserLastToken = nextToken;
  return vector_peek(currentProcess->tokenVec);
}
static token *tokenPeekNext() {
  token *nextToken = vector_peek_no_increment(currentProcess->tokenVec);
  parserIgnoreNlOrComment(nextToken);
  return vector_peek_no_increment(currentProcess->tokenVec);
}

void parseSingleTokenToNode() {
  token *token = tokenNext();
  node *node = NULL;
  switch (token->type) {
  case NUMBER:
    node = nodeCreate(
        &(struct node){.type = NODE_TYPE_NUMBER, .llnum = token->llnum});
    break;
  case IDENTIFIER:
    node = nodeCreate(
        &(struct node){.type = NODE_TYPE_IDENTIFIER, .sval = token->sval});
    break;
  case STRING:
    node = nodeCreate(
        &(struct node){.type = NODE_TYPE_STRING, .sval = token->sval});

    break;
  default:
    compilerError(currentProcess, "This token cannot be converted into a node");
    errorHappened = 1;
  }
}

int parseNext() {

  token *token = tokenPeekNext();
  if (!token) {
    // check memory cleanup
    return -1;
  }

  int res = 0;

  switch (token->type) {
  case NUMBER:
  case IDENTIFIER:
  case STRING:
    parseSingleTokenToNode();
    break;
  }

  return 0;
}

int parse(compileProcess *process) {
  currentProcess = process;
  parserLastToken = NULL;
  nodeSetVector(process->nodeVec, process->nodeTreeVec);
  struct node *node = NULL;

  vector_set_peek_pointer(process->tokenVec, 0);
  while (parseNext() == 0) {
    node = nodePeek();
    vector_push(process->nodeTreeVec, &node);
  }
  if (errorHappened != 0) {
    return PARSE_ERROR;
  }
  return PARSE_ALL_OK;
}
