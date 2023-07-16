#include "compileProcess.h"
#include "compiler.h"
#include "node.h"
#include "position.h"
#include "token.h"
#include "vector.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static struct compileProcess *currentProcess;
static token *parserLastToken;
extern struct node *parserCurrentBody;
extern struct expressionableOpPrecedenceGroup
    opPrecedence[TOTAL_OPERATOR_GROUPS];

enum {
  PARSER_SCOPE_ENTITY_ON_STACK = 0b00000001,
  PARSER_SCOPE_ENTITY_STRUCTURE_SCOPE = 0b00000011

};

struct parserScopeEntity {
  int flags;
  int stackOffset;
  node *node;
};

struct parserScopeEntity *parserScopeEntityNew(node *node, int stackOffset,
                                               int flags) {
  struct parserScopeEntity *entity =
      calloc(1, sizeof(struct parserScopeEntity));
  entity->node = node;
  entity->flags = flags;
  entity->stackOffset = stackOffset;
  vector_push(currentProcess->gb, &entity);
  return entity;
}

enum { HISTORY_FLAG_INSIDE_UNION = 0b00000001 };

typedef struct history {
  int flags;
} history;

int parseKeyword(struct history *hs);
history *historyBegin(int flags) {
  history *history = calloc(1, sizeof(struct history));
  history->flags = flags;
  vector_push(currentProcess->gb, &history);
  return history;
}
history *historyDown(history *hs, int flags) {
  history *newHistory = calloc(1, sizeof(history));
  memcpy(newHistory, hs, sizeof(history));
  newHistory->flags = flags;
  vector_push(currentProcess->gb, &newHistory);
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
  // maybe needs to change
  if (!nextToken) {
    return NULL;
  }
  parserIgnoreNlOrComment(nextToken);
  currentProcess->position = nextToken->position;
  parserLastToken = nextToken;
  token *toReturn = vector_peek(currentProcess->tokenVec);
  return toReturn;
}
static token *tokenPeekNext() {
  token *nextToken = vector_peek_no_increment(currentProcess->tokenVec);
  parserIgnoreNlOrComment(nextToken);
  return vector_peek_no_increment(currentProcess->tokenVec);
}

static bool tokenNextIsSymbol(char c) {
  struct token *token = tokenPeekNext();
  return tokenIsSymbol(token, c);
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
  vector_push(currentProcess->gb, &secondaryDataType);
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
  }
  parserDatatypeAdjustSizeForSecondary(datatypeOut, datatypeSecToken);
}
void parserInitDatatypeTypeAndSize(token *datatypeToken,
                                   token *datatypeSecToken, datatype *typeOut,
                                   int ptrDepth, int expectedType) {

  if (!secondaryTypeAllowed(expectedType) && datatypeSecToken) {
    compilerError(currentProcess, "You provided an invalid secondary type \n");
  }
  switch (expectedType) {
  case DATA_TYPE_EXPECT_PRIMITIVE:
    parserDatatypeInitSizeAndTypeForPrimitive(datatypeToken, datatypeSecToken,
                                              typeOut);
    break;
  case DATA_TYPE_EXPECT_STRUCT:
  case DATA_TYPE_EXPECT_UNION:
    compilerError(currentProcess, "Cannot use struct or unions yet \n");
    break;
  default:
    compilerError(currentProcess, "Unsupported datatype expectation \n");
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
void parserIgnoreInt(datatype *datatype);

void parseExpressionableRoot(history *hs) {
  parseExpressionable(hs);
  node *resultNode = nodePop();
  nodePush(resultNode);
}
void makeVariableNode(datatype *dtype, token *name, node *valueNode) {
  const char *nameStr = NULL;
  if (name) {
    nameStr = name->sval;
  }

  node *f = nodeCreate(&(struct node){.type = NODE_TYPE_VARIABLE,
                                      .var.name = nameStr,
                                      .var.val = valueNode,
                                      .var.type = *dtype});
}
static void expectOp(const char *op) {
  struct token *nextToken = tokenNext();
  if (!nextToken || nextToken->type != OPERATOR || !S_EQ(nextToken->sval, op)) {
    compilerError(
        currentProcess,
        "Expecting the operator %s, but something else was provided\n",
        nextToken->sval);
  }
}
static void expectSym(char c);
struct arrayBrackets *parseArrayBrackets(struct history *hs) {
  struct arrayBrackets *brackets = arrayBracketsNew();
  while (tokenNextIsOperator("[")) {
    expectOp("[");
    if (tokenIsSymbol(tokenPeekNext(), ']')) {
      expectSym(']');
      break;
    }
    parseExpressionableRoot(hs);
    expectSym(']');
    node *expNode = nodePop();
    makeBracketNode(expNode);
    node *bracketNode = nodePop();
    arrayBracketsAdd(brackets, bracketNode);
  }
  return brackets;
}
void makeVariableNodeAndRegister(history *hs, datatype *dtype, token *name,
                                 node *valueNode) {
  makeVariableNode(dtype, name, valueNode);
  node *varNode = nodePop();
  // cleanup
  nodePush(varNode);
}
void parseVariable(datatype *dtype, token *namedToken, history *hs) {
  node *valueNode = NULL;
  struct arrayBrackets *brackets = NULL;
  if (tokenNextIsOperator("[")) {
    brackets = parseArrayBrackets(hs);
    dtype->array.brackets = brackets;
    dtype->array.size = arrayBracketsCalcSize(dtype, brackets);
    dtype->flags |= DATATYPE_FLAG_IS_ARRAY;
  }
  if (tokenNextIsOperator("=")) {
    // ignore =
    //  int x = 50;
    //  x 50
    tokenNext();
    parseExpressionableRoot(hs);
    valueNode = nodePop();
  }
  makeVariableNodeAndRegister(hs, dtype, namedToken, valueNode);
}
static void expectSym(char c) {
  token *nextToken = tokenNext();
  if (!nextToken || nextToken->type != SYMBOL || nextToken->cval != c) {
    compilerError(currentProcess,
                  "Expected symbol %c but something else was provided \n", c);
  }
}
void parseSymbol() {
  compilerError(currentProcess, "Symbols are not yet supported\n");
}
void parseStatement(history *hs) {
  if (tokenPeekNext()->type == KEYWORD) {
    parseKeyword(hs);
    return;
  }
  parseExpressionableRoot(hs);
  if (tokenPeekNext()->type == SYMBOL && !tokenIsSymbol(tokenPeekNext(), ';')) {
    parseSymbol();
    return;
  }
  expectSym(';');
}
void makeVariableListNode(struct vector *varlistVec) {
  node *toCreate = nodeCreate(&(struct node){.type = NODE_TYPE_VARIABLE_LIST});

  toCreate->varlist.list = varlistVec;
}
void parserFinaliseBody(history *hs, node *bodyNode, struct vector *bodyVec,
                        size_t *varSize, node *largestAlignEligibleNode,
                        node *largestPossibleVarNode) {
  if (hs->flags & HISTORY_FLAG_INSIDE_UNION) {
    if (largestPossibleVarNode) {
      *varSize = variableSize(largestPossibleVarNode);
    }
  }
  int padding = computeSumPadding(bodyVec);
  *varSize += padding;
  if (largestAlignEligibleNode) {
    *varSize = alignValue(*varSize, largestAlignEligibleNode->var.type.size);
  }
  bool padded = padding != 0;

  bodyNode->body.largestVarNode = largestAlignEligibleNode;
  bodyNode->body.padded = false;
  bodyNode->body.size = *varSize;
  bodyNode->body.statements = bodyVec;
}
void parserScopeNew() { scopeNew(currentProcess, 0); }

void parserScopeFinish(compileProcess *currentProcess) {
  scopeFinish(currentProcess);
}

void parserScopePush(node *node, size_t size) {
  scopePush(currentProcess, node, size);
}

void parserAppendSizeForNodeStructUnion(history *hs, size_t *varSize,
                                        struct node *node) {
  *varSize += variableSize(node);
  if (node->var.type.flags & DATATYPE_FLAG_IS_POINTER) {
    return;
  }
  struct node *largestVarNode =
      variableStructOrUnionBodyNode(node)->body.largestVarNode;
  if (largestVarNode) {
    *varSize += alignValue(*varSize, largestVarNode->var.type.size);
  }
}
void parserAppendSizeForNode(history *hs, size_t *varSize, node *node);
void parserAppendSizeForVariableList(history *hs, size_t *varSize,
                                     struct vector *vec) {
  vector_set_peek_pointer(vec, 0);
  node *node = vector_peek_ptr(vec);
  while (node) {
    parserAppendSizeForNode(hs, varSize, node);
    node = vector_peek_ptr(vec);
  }
}
void parserAppendSizeForNode(history *hs, size_t *varSize, node *node) {
  compilerWarning(currentProcess,
                  "Parsing size tracking is not yet supported\n");
  if (!node) {
    return;
  }
  if (node->type == NODE_TYPE_VARIABLE) {
    if (nodeIsStructOrUnionVariable(node)) {
      parserAppendSizeForNodeStructUnion(hs, varSize, node);
      return;
    }
    *varSize += variableSize(node);
  } else if (node->type == NODE_TYPE_VARIABLE_LIST) {
    parserAppendSizeForVariableList(hs, varSize, node->varlist.list);
  }
}
void parseBodySingleStatement(size_t *varSize, struct vector *bodyVec,
                              history *hs) {
  makeBodyNode(bodyVec, 0, false, NULL);
  node *bodyNode = nodePop();
  bodyNode->binded.owner = parserCurrentBody;
  parserCurrentBody = bodyNode;
  node *stmtNode = NULL;
  parseStatement(historyDown(hs, hs->flags));
  stmtNode = nodePop();
  vector_push(bodyVec, &stmtNode);
  parserAppendSizeForNode(hs, varSize, stmtNode);
  node *largestVarNode = NULL;
  if (stmtNode->type == NODE_TYPE_VARIABLE) {
    largestVarNode = stmtNode;
  }
  parserFinaliseBody(hs, bodyNode, bodyVec, varSize, largestVarNode,
                     largestVarNode);
  parserCurrentBody = bodyNode->binded.owner;
  nodePush(bodyNode);
}
void parseBody(size_t *variableSize, history *hs) {
  parserScopeNew();
  size_t tmpSize = 0x00;
  if (!variableSize) {
    variableSize = &tmpSize;
  }
  struct vector *bodyVec = vector_create(sizeof(struct node *));
  if (!tokenNextIsSymbol('{')) {
    parseBodySingleStatement(variableSize, bodyVec, hs);
    parserScopeFinish(currentProcess);
    return;
  }
  parserScopeFinish(currentProcess);
}

void parseStructNoNewScope(datatype *type) {}
void parseStruct(struct datatype *dtype) {
  bool isForwardDecleration = !tokenIsSymbol(tokenPeekNext(), '{');
  if (!isForwardDecleration) {
    parserScopeNew();
  }
  parseStructNoNewScope(dtype);
  if (!isForwardDecleration) {
    parserScopeFinish(currentProcess);
  }
}
void parseStructOrUnion(struct datatype *dtype) {
  switch (dtype->type) {
  case DATA_TYPE_EXPECT_STRUCT:
    parseStruct(dtype);
    break;
  case DATA_TYPE_UNION:
    break;
  default:
    compilerError(currentProcess,
                  "The provided data type is not a struct or a union\n");
  }
}
void parseVariableFunctionStructUnion(struct history *hs) {
  datatype dtype;
  parseDatatype(&dtype);

  if (dataTypeIsStructOrUnion(&dtype) && tokenNextIsSymbol('{')) {
  }

  parserIgnoreInt(&dtype);
  token *namedToken = tokenNext();
  if ((namedToken && namedToken->type != IDENTIFIER) || (!namedToken)) {
    compilerError(currentProcess,
                  "Expecting a valid name after variable decleration\n");
  }
  parseVariable(&dtype, namedToken, hs);
  if (tokenIsOperator(tokenPeekNext(), ",")) {
    struct vector *varList = vector_create(sizeof(struct node *));
    node *varNode = nodePop();
    vector_push(varList, &varNode);
    while (tokenIsOperator(tokenPeekNext(), ",")) {
      tokenNext();
      namedToken = tokenNext();
      parseVariable(&dtype, namedToken, hs);
      varNode = nodePop();
      vector_push(varList, &varNode);
    }
    makeVariableListNode(varList);
  }
  expectSym(';');
}
bool parserIsIntValidAfterDatatype(datatype *type) {
  return type->type == DATA_TYPE_LONG || type->type == DATA_TYPE_FLOAT ||
         type->type == DATA_TYPE_DOUBLE;
}
void parserIgnoreInt(datatype *datatype) {
  if (!tokenIsKeyword(tokenPeekNext(), "int")) {
    return;
  }
  if (!parserIsIntValidAfterDatatype(datatype)) {
    compilerError(currentProcess, "Not valid int decleration \n");
  }
  tokenNext();
}
int parseKeyword(struct history *hs) {
  token *token = tokenPeekNext();
  if (isKeywordVariableModifier(token->sval) ||
      keywordIsDataType(token->sval)) {
    parseVariableFunctionStructUnion(hs);
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
}
void parseKeywordForGlobal() {
  parseKeyword(historyBegin(0));
  node *node = nodePop();
  nodePush(node);
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

  createRootScope(process);
  currentProcess = process;
  parserLastToken = NULL;
  nodeSetVector(process->nodeVec, process->nodeTreeVec,
                process->nodeGarbageVec);

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
  return PARSE_ALL_OK;
}
