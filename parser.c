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

extern struct expressionableOpPrecedenceGroup
    opPrecedence[TOTAL_OPERATOR_GROUPS];

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
  return token && token->type == SYMBOL && token->cval == c;
}

static bool tokenIsNewLineOrComment(token *token) {
  if (!token) {
    return false;
  }
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
static int
getPrecedenceForOp(const char *op,
                   struct expressionableOpPrecedenceGroup **opGroup) {

  *opGroup = NULL;
  for (int i = 0; i < TOTAL_OPERATOR_GROUPS; i++) {
    for (int x = 0; opPrecedence[i].operators[x]; x++) {
      const char *_op = opPrecedence[i].operators[x];
      if (S_EQ(op, _op)) {
        *opGroup = &opPrecedence[i];
        return i;
      }
    }
  }
  return -1;
}
static bool parserLeftOpHasPriority(const char *opleft, const char *opright) {
  struct expressionableOpPrecedenceGroup *groupLeft = NULL;
  struct expressionableOpPrecedenceGroup *groupRight = NULL;

  if (S_EQ(opleft, opright)) {
    return false;
  }
  int precedenceLeft = getPrecedenceForOp(opleft, &groupLeft);
  int precedenceRight = getPrecedenceForOp(opright, &groupRight);
  if (groupLeft->associtivity == ASSOCIATIVITY_RIGHT_TO_LEFT) {
    return false;
  }
  return precedenceLeft <= precedenceRight;
}
node *nodeShiftChildrenLeft(node *node) {
  if (node->type != NODE_TYPE_EXPRESSION ||
      node->exp.right->type != NODE_TYPE_EXPRESSION) {
    errorHappened = 1;
    compilerError(currentProcess, "node is not an expression");
  }
  const char *rightOp = node->exp.right->exp.op;
  struct node *newNodeLeftExp = node->exp.left;
  struct node *newNodeRight = node->exp.right->exp.left;
  struct node *newLeftOperand =
      makeExpNode(newNodeLeftExp, newNodeRight, node->exp.op);
  struct node *newRightOperand = node->exp.right->exp.right;
  node->exp.left = newLeftOperand;
  node->exp.right = newRightOperand;
  node->exp.op = rightOp;
  return node;
}
void parserReorderExpressionNode(struct node **nodeOut) {
  node *node = *nodeOut;
  if (node->type != NODE_TYPE_EXPRESSION) {
    return;
  }
  // no expressions
  if (node->exp.left->type != NODE_TYPE_EXPRESSION && node->exp.right &&
      node->exp.right->type != NODE_TYPE_EXPRESSION) {
    return;
  }

  if (node->exp.left->type != NODE_TYPE_EXPRESSION && node->exp.right &&
      node->exp.right->type == NODE_TYPE_EXPRESSION) {
    const char *rightOp = node->exp.right->exp.op;
    if (parserLeftOpHasPriority(node->exp.op, rightOp)) {
      struct node *nodeC = nodeShiftChildrenLeft(node);
      parserReorderExpressionNode(&nodeC->exp.left);
      parserReorderExpressionNode(&nodeC->exp.right);
    }
  }
}

int parseExpressionNormal(history *hs) {
  token *opToken = tokenPeekNext();
  char *op = opToken->sval;
  node *nodeLeft = nodePeekExpressionableOrNull();
  if (!nodeLeft) {
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
  node *lalal = makeExpNode(nodeLeft, rightNode, op);

  node *expNode = nodePop();
  parserReorderExpressionNode(&expNode);
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
void parseIdentifier(history *hs) {
  if (tokenPeekNext()->type != IDENTIFIER) {
    errorHappened = 1;
    compilerError(currentProcess, "Token is not an identifier");
  }
  parseSingleTokenToNode();
}
static bool isKeywordVariableModifier(const char *val) {
  return S_EQ(val, "unsigned") || S_EQ(val, "signed") || S_EQ(val, "static") ||
         S_EQ(val, "const") || S_EQ(val, "extern") ||
         S_EQ(val, "__ignore_typecheck__");
}
bool keywordIsDataType(const char *str) {
  return S_EQ(str, "void") || S_EQ(str, "char") || S_EQ(str, "int") ||
         S_EQ(str, "short") || S_EQ(str, "float") || S_EQ(str, "double") ||
         S_EQ(str, "long") || S_EQ(str, "struct") || S_EQ(str, "union");
}

void parseDatatypeModifiers(datatype *type) {
  token *token = tokenPeekNext();
  while (token && token->type == KEYWORD) {
    if (!isKeywordVariableModifier(token->sval)) {
      break;
    }
    if (S_EQ(token->sval, "signed")) {
      type->flags |= DATATYPE_FLAG_IS_SIGNED;
    } else if (S_EQ(token->sval, "unsigned")) {
      type->flags &= ~DATATYPE_FLAG_IS_SIGNED;
    } else if (S_EQ(token->sval, "static")) {
      type->flags |= DATATYPE_FLAG_IS_STATIC;
    } else if (S_EQ(token->sval, "const")) {
      type->flags |= DATATYPE_FLAG_IS_CONST;
    } else if (S_EQ(token->sval, "extern")) {
      type->flags |= DATATYPE_FLAG_IS_EXTERN;
    } else if (S_EQ(token->sval, "__ignore_typecheck__")) {
      type->flags |= DATATYPE_FLAG_IGNORE_TYPE_CHECKING;
    }
    tokenNext();
    token = tokenPeekNext();
  }
}
void parserGetDatatypeTokens(token **typeToken, token **secTypeToken) {
  *typeToken = tokenNext();
  token *nextToken = tokenPeekNext();
  if (tokenIsPrimitiveKeyword(nextToken)) {
    *secTypeToken = nextToken;
    tokenNext();
  }
}
int parserDatatypeExpectedForTypeString(const char *str) {
  int type = DATA_TYPE_EXPECT_PRIMITIVE;
  if (S_EQ(str, "union")) {
    type = DATA_TYPE_EXPECT_UNION;
  }
  if (S_EQ(str, "struct")) {
    type = DATA_TYPE_EXPECT_STRUCT;
  }
  return type;
}
int parserGetRandomIndex() {
  static int x = 0;
  x++;
  return x;
}
token *parserBuildRandomStructName() {
  char tmpName[25];
  sprintf(tmpName, "customtypename_%i", parserGetRandomIndex());
  char *sval = malloc(sizeof(tmpName));
  strncpy(sval, tmpName, sizeof(tmpfile));
  struct token *token = calloc(1, sizeof(struct token));
  token->type = IDENTIFIER;
  token->sval = sval;
  return token;
}
static bool tokenNextIsOperator(const char *op) {
  token *token = tokenPeekNext();
  return tokenIsOperator(token, op);
}
int getPtrDepth() {
  int depth = 0;
  while (tokenNextIsOperator("*")) {
    depth++;
    tokenNext();
  }
  return depth;
}
bool secondaryTypeAllowed(int expectedType) {
  return expectedType == DATA_TYPE_EXPECT_PRIMITIVE;
}
bool datatypeIsSecondaryAllowedForType(const char *type) {
  return S_EQ(type, "long") || S_EQ(type, "short") || S_EQ(type, "double") ||
         S_EQ(type, "float");
}

void parserDatatypeInitSizeAndTypeForPrimitive(struct token *datatypeToken,
                                               struct token *datatypeSecToken,
                                               datatype *datatypeOut);
void parserDatatypeAdjustSizeForSecondary(datatype *datatype,
                                          token *datatypeSecToken) {
  if (!datatypeSecToken) {
    return;
  }
  struct datatype *secondaryDataType = calloc(1, sizeof(struct datatype));

  parserDatatypeInitSizeAndTypeForPrimitive(datatypeSecToken, NULL,

                                            secondaryDataType);
  vector_push(currentProcess->garbageDatatypes, &secondaryDataType);
  datatype->size += secondaryDataType->size;
  datatype->secondary = secondaryDataType;
  datatype->flags |= DATATYPE_FLAG_IS_SECONDARY;
}
void parserDatatypeInitSizeAndTypeForPrimitive(struct token *datatypeToken,
                                               struct token *datatypeSecToken,
                                               datatype *datatypeOut) {
  if (!datatypeIsSecondaryAllowedForType(datatypeToken->sval) &&
      datatypeSecToken) {
    compilerError(
        currentProcess,
        "You cannot use a secondary datatype for the given datatype %s \n",
        datatypeToken->sval);
    errorHappened = 1;
  }
  if (S_EQ(datatypeToken->sval, "void")) {
    datatypeOut->type = DATA_TYPE_VOID;
    datatypeOut->size = DATA_SIZE_ZERO;
  } else if (S_EQ(datatypeToken->sval, "char")) {
    datatypeOut->type = DATA_TYPE_CHAR;
    datatypeOut->size = DATA_SIZE_BYTE;
  } else if (S_EQ(datatypeToken->sval, "short")) {
    datatypeOut->type = DATA_TYPE_SHORT;
    datatypeOut->size = DATA_SIZE_WORD;
  } else if (S_EQ(datatypeToken->sval, "int")) {
    datatypeOut->type = DATA_TYPE_INTEGER;
    datatypeOut->size = DATA_SIZE_DWORD;
  } else if (S_EQ(datatypeToken->sval, "long")) {
    datatypeOut->type = DATA_TYPE_LONG;
    datatypeOut->size = DATA_SIZE_DWORD;
  } else if (S_EQ(datatypeToken->sval, "float")) {
    datatypeOut->type = DATA_TYPE_FLOAT;
    datatypeOut->size = DATA_SIZE_DWORD;
  } else if (S_EQ(datatypeToken->sval, "double")) {
    datatypeOut->type = DATA_TYPE_DOUBLE;
    datatypeOut->size = DATA_SIZE_DWORD;
  } else {
    compilerError(currentProcess, "Invalid primitive data type \n");
    errorHappened = 1;
  }
  parserDatatypeAdjustSizeForSecondary(datatypeOut, datatypeSecToken);
}
void parserInitDatatypeTypeAndSize(token *datatypeToken,
                                   token *datatypeSecToken, datatype *typeOut,
                                   int ptrDepth, int expectedType) {

  if (!secondaryTypeAllowed(expectedType) && datatypeSecToken) {
    compilerError(currentProcess, "You provided an invalid secondary type \n");
    errorHappened = 1;
  }
  switch (expectedType) {
  case DATA_TYPE_EXPECT_PRIMITIVE:
    parserDatatypeInitSizeAndTypeForPrimitive(datatypeToken, datatypeSecToken,
                                              typeOut);
    break;
  case DATA_TYPE_EXPECT_STRUCT:
  case DATA_TYPE_EXPECT_UNION:
    compilerError(currentProcess, "Cannot use struct or unions yet \n");
    errorHappened = 1;
    break;
  default:
    compilerError(currentProcess, "Unsupported datatype expectation \n");
    errorHappened = 1;
  }
}
void parserDatatypeInit(token *datatypeToken, token *datatypeSecToken,
                        datatype *typeOut, int ptrDepth, int expectedType) {
  parserInitDatatypeTypeAndSize(datatypeToken, datatypeSecToken, typeOut,
                                ptrDepth, expectedType);
  typeOut->typeStr = datatypeToken->sval;
  if (S_EQ(datatypeToken->sval, "long") && datatypeSecToken &&
      S_EQ(datatypeSecToken->sval, "long")) {
    compilerWarning(currentProcess, "Warning: Our compiler doesnt support 64 "
                                    "bit longs, defaulting to 32 \n");
    typeOut->size = DATA_SIZE_DWORD;
  }
}
void parseDatatypeType(datatype *type) {
  token *datatypeToken = NULL;
  token *datatypeSecToken = NULL;
  parserGetDatatypeTokens(&datatypeToken, &datatypeSecToken);
  int expectedType = parserDatatypeExpectedForTypeString(datatypeToken->sval);
  if (datatypeIsStructOrUnionForName(datatypeToken->sval)) {
    if (tokenPeekNext()->type == IDENTIFIER) {
      // ex. struct abc. now the datatype is abc
      datatypeToken = tokenNext();
    } else {
      // struct without name
      // FREE TOKEN
      datatypeToken = parserBuildRandomStructName();
      type->flags |= DATATYPE_FLAG_STRUCT_UNION_NO_NAME;
    }
  }
  int ptrDepth = getPtrDepth();
  parserDatatypeInit(datatypeToken, datatypeSecToken, type, ptrDepth,
                     expectedType);
}
void parseDatatype(datatype *dtype) {
  memset(dtype, 0, sizeof(datatype));
  // in C everything is signed by default
  dtype->flags |= DATATYPE_FLAG_IS_SIGNED;
  parseDatatypeModifiers(dtype);
  parseDatatypeType(dtype);
  parseDatatypeModifiers(dtype);
}
void parseVariableFunctionStructUnion(struct history *hs) {
  datatype dtype;
  parseDatatype(&dtype);
}
int parseKeyword(struct history *hs) {
  token *token = tokenPeekNext();
  if (isKeywordVariableModifier(token->sval) ||
      keywordIsDataType(token->sval)) {
    parseVariableFunctionStructUnion(hs);
    free(hs);
    return 0;
  }

  // we ll see, maybe free it with garbage vec
  return 0;
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
  case IDENTIFIER:
    parseIdentifier(hs);
    break;
  case OPERATOR:
    res = parseExp(hs);
    break;
  case KEYWORD:
    parseKeyword(hs);
    break;
  }
  return res;
}
void parseExpressionable(history *hs) {
  while (parseExpressionableSingle(hs) == 0) {
  }
  free(hs);
}
void parseKeywordForGlobal() {
  parseKeyword(historyBegin(0));
  node *node = nodePop();
};
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
    parseExpressionable(historyBegin(0));
    break;
  case KEYWORD:
    parseKeywordForGlobal();
    res = 0;
    break;
  }
  return res;
}

int parse(compileProcess *process) {
  currentProcess = process;
  parserLastToken = NULL;
  nodeSetVector(process->nodeVec, process->nodeTreeVec, process->garbageVec);
  struct node *node = NULL;

  vector_set_peek_pointer(process->tokenVec, 0);
  while (parseNext() == 0) {
    // MAYBE USE nodePeek()
    node = nodePeekOrNull();
    if (node) {
      vector_push(process->nodeTreeVec, &node);
    }
  }

  vector_set_peek_pointer(currentProcess->nodeVec, 0);
  struct node **nodeToClean = (struct node **)vector_peek(process->nodeVec);
  if (errorHappened != 0) {
    return PARSE_ERROR;
  }
  return PARSE_ALL_OK;
}
