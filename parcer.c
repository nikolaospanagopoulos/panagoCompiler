#include "compileProcess.h"
#include "compiler.h"
#include "node.h"
#include "position.h"
#include "token.h"
#include "vector.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int errorHappened = 0;
static struct compileProcess *currentProcess;
static token *parserLastToken;

typedef struct history {
  int flags;
} history;

history *historyBegin(int flags) {
  history *history = calloc(1, sizeof(struct history));
  history->flags = flags;
  return history;
}
history *historyDown(history *hs, int flags) {
  history *newHistory = calloc(1, sizeof(history));
  memcpy(newHistory, hs, sizeof(history));
  newHistory->flags = flags;
  return newHistory;
}

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
void parseExpressionable(history *hs);
void parseExpressionableForOp(history *hs, const char *op);

int parseExpressionNormal(history *hs) {
  token *opToken = tokenPeekNext();
  char *op = opToken->sval;
  node *nodeLeft = nodePeekExpressionableOrNull();
  if (!nodeLeft) {
    printf("no node lef \n");
    return -1;
  }
  tokenNext();
  nodePop();
  nodeLeft->flags |= INSIDE_EXPRESSION;
  parseExpressionableForOp(historyDown(hs, hs->flags), op);
  node *rightNode = nodePop();
  if (!rightNode) {
    return -1;
  }
  rightNode->flags |= INSIDE_EXPRESSION;
  makeExpNode(nodeLeft, rightNode, op);
  node *expNode = nodePop();
  nodePush(expNode);
  return 0;
}
void parseExpressionableForOp(history *hs, const char *op) {
  parseExpressionable(hs);
}
int parseExp(history *hs) {
  int res = parseExpressionNormal(hs);
  return res;
}

int parseExpressionableSingle(history *hs) {
  token *token = tokenPeekNext();
  if (!token) {
    return -1;
  }
  hs->flags |= INSIDE_EXPRESSION;
  int res = -1;
  switch (token->type) {
  case NUMBER:
    parseSingleTokenToNode();
    res = 0;
    break;
  case OPERATOR:

    res = parseExp(hs);
    break;
  }
  return res;
}
void parseExpressionable(history *hs) {
  while (parseExpressionableSingle(hs) == 0) {
  }
  free(hs);
}
int parseNext() {

  token *token = tokenPeekNext();
  if (!token) {
    // check memory cleanup
    printf("end\n");
    return -1;
  }

  int res = 0;

  switch (token->type) {
  case NUMBER:
  case IDENTIFIER:
  case STRING:
    parseExpressionable(historyBegin(0));
    break;
  }
  return res;
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
