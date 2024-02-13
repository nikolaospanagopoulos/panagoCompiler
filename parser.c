#include "compileProcess.h"
#include "compiler.h"
#include "fixup.h"
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
static struct fixupSystem *parserFixupSystem;
static token *parserLastToken;
extern struct node *parserCurrentBody;
extern struct expressionableOpPrecedenceGroup
    opPrecedence[TOTAL_OPERATOR_GROUPS];
extern struct node *parserCurrentFunction;
struct node *parserBlancNode;
enum {
  PARSER_SCOPE_ENTITY_ON_STACK = 0b00000001,
  PARSER_SCOPE_ENTITY_STRUCTURE_SCOPE = 0b00000011

};

void parseDatatype(datatype *dtype);
struct parserScopeEntity {
  int flags;
  int stackOffset;
  node *node;
};

int getPtrDepth();
void parserDealWithAdditionalExpression();
static void expectSym(char c);
static void expectOp(const char *op);
void parserScopeNew();
void parseCast();
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

enum {
  HISTORY_FLAG_INSIDE_UNION = 0b00000001,
  HISTORY_FLAG_IS_UPWARD_STACK = 0b00000010,
  HISTORY_FLAG_IS_GLOBAL_SCOPE = 0b00000100,
  HISTORY_FLAG_INSIDE_STRUCT = 0b00001000,
  HISTORY_FLAG_INSIDE_FUNCTION_BODY = 0b00010000,
  HISTORY_FLAG_INSIDE_SWITCH_STMT = 0b00100000,
  HISTORY_FLAG_PARENTHESES_NOT_A_FUNCTION_CALL = 0b01000000
};
struct historyCases {
  struct vector *cases;
  bool hasDefaultCase;
};
typedef struct history {
  int flags;
  struct parserHistorySwitch {
    struct historyCases caseData;
  } _switch;
} history;

void parseForParentheses(history *hs);
struct parserHistorySwitch parserNewSwitchStmt(history *hs) {
  // TODO::: MAYBE CHANGE
  memset(&hs->_switch, 0, sizeof(hs->_switch));
  hs->_switch.caseData.cases = vector_create(sizeof(struct parsedSwitchedCase));
  vector_push(currentProcess->gbForVectors, &hs->_switch.caseData.cases);
  hs->flags |= HISTORY_FLAG_INSIDE_SWITCH_STMT;
  return hs->_switch;
}
void parserRegisterCase(history *hs, struct node *caseNode) {
  if (!(hs->flags & HISTORY_FLAG_INSIDE_SWITCH_STMT)) {
    compilerError(currentProcess, "Not inside switch statement\n");
  }
  struct parsedSwitchedCase scase;
  scase.index = caseNode->stmt._case.exp->llnum;
  vector_push(hs->_switch.caseData.cases, &scase);
}
void parseExpressionableRoot(history *hs);
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
  parserIgnoreNlOrComment(nextToken);
  if (nextToken) {
    currentProcess->position = nextToken->position;
  }
  parserLastToken = nextToken;
  return vector_peek(currentProcess->tokenVec);
}
static token *tokenPeekNext() {
  token *nextToken = vector_peek_no_increment(currentProcess->tokenVec);
  parserIgnoreNlOrComment(nextToken);
  return vector_peek_no_increment(currentProcess->tokenVec);
}

static bool tokenNextIsKeyword(const char *keyword) {
  token *token = tokenPeekNext();
  return tokenIsKeyword(token, keyword);
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
void parserNodeMoveRightLeftToLeft(struct node *node) {
  makeExpNode(node->exp.left, node->exp.right->exp.left, node->exp.op);
  struct node *completedNode = nodePop();
  const char *newOp = node->exp.right->exp.op;
  node->exp.left = completedNode;
  node->exp.right = node->exp.right->exp.right;
  node->exp.op = newOp;
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
      nodeShiftChildrenLeft(node);
      parserReorderExpressionNode(&node->exp.left);

      parserReorderExpressionNode(&node->exp.right);
    }
  }
  if ((isArrayNode(node->exp.left) && isNodeAssignment(node->exp.right)) ||
      (nodeIsExpression(node->exp.left, "()") ||
       nodeIsExpression(node->exp.left, "[]")) &&
          nodeIsExpression(node->exp.right, ",")) {
    parserNodeMoveRightLeftToLeft(node);
  }
}
bool parserIsUnaryOperator(const char *op) { return isUnaryOperator(op); }
void parseForIndirectionUnary() {
  int depth = getPtrDepth();
  parseExpressionable(historyBegin(0));
  struct node *unaryOperandNode = nodePop();
  makeUnaryNode("*", unaryOperandNode);

  struct node *unaryNode = nodePop();
  unaryNode->unary.indirection.depth = depth;
  nodePush(unaryNode);
}

void parseForNormalUnary() {
  const char *unaryOp = tokenNext()->sval;
  parseExpressionable(historyBegin(0));
  struct node *unaryOperandNode = nodePop();
  makeUnaryNode(unaryOp, unaryOperandNode);
}

void parseForUnary() {
  const char *unaryOp = tokenPeekNext()->sval;
  if (opIsIndirection(unaryOp)) {
    parseForIndirectionUnary();
    return;
  }
  parseForNormalUnary();
  parserDealWithAdditionalExpression();
}
int parseExpressionNormal(history *hs) {
  token *opToken = tokenPeekNext();
  char *op = opToken->sval;
  node *nodeLeft = nodePeekExpressionableOrNull();
  if (!nodeLeft) {
    if (!parserIsUnaryOperator(op)) {
      compilerError(currentProcess, "The given expression has no left operand");
    }
    parseForUnary();
    return 0;
  }
  tokenNext();
  nodePop();
  nodeLeft->flags |= INSIDE_EXPRESSION;
  if (tokenPeekNext()->type == OPERATOR) {
    if (S_EQ(tokenPeekNext()->sval, "(")) {
      parseForParentheses(historyDown(
          hs, hs->flags | HISTORY_FLAG_PARENTHESES_NOT_A_FUNCTION_CALL));
    } else if (parserIsUnaryOperator(tokenPeekNext()->sval)) {
      parseForUnary();
    } else {
      compilerError(
          currentProcess,
          "Two operators are expected for a given expression for operator %s\n",
          tokenPeekNext()->sval);
    }
  } else {

    parseExpressionableForOp(historyDown(hs, hs->flags), op);
  }
  node *rightNode = nodePop();
  if (!rightNode) {
    return -1;
  }
  rightNode->flags |= INSIDE_EXPRESSION;
  makeExpNode(nodeLeft, rightNode, op);

  node *expNode = nodePop();
  parserReorderExpressionNode(&expNode);
  nodePush(expNode);

  return 0;
}
void parseExpressionableForOp(history *hs, const char *op) {
  parseExpressionable(hs);
}
void parserDealWithAdditionalExpression() {
  if (tokenPeekNext()->type == OPERATOR) {
    parseExpressionable(historyBegin(0));
  }
}
void parseForParentheses(history *hs) {
  expectOp("(");
  if (tokenPeekNext()->type == KEYWORD) {
    parseCast();
    return;
  }
  struct node *leftNode = NULL;
  struct node *tmpNode = nodePeekOrNull();
  if (tmpNode && nodeIsValueType(tmpNode)) {
    leftNode = tmpNode;
    nodePop();
  }
  struct node *expNode = parserBlancNode;
  if (!tokenNextIsSymbol(')')) {
    parseExpressionableRoot(historyBegin(0));
    expNode = nodePop();
  }
  expectSym(')');
  makeExpParenthesisNode(expNode);
  if (leftNode) {
    struct node *parenthesisNode = nodePop();
    makeExpNode(leftNode, parenthesisNode, "()");
  }
  parserDealWithAdditionalExpression();
}
void parseTenary(history *hs);
void parseForComma(history *hs) {
  tokenNext();
  struct node *left = nodePop();
  parseExpressionableRoot(hs);
  struct node *right = nodePop();
  makeExpNode(left, right, ",");
}
void parseForArray(history *hs) {
  struct node *left = nodePeekOrNull();
  if (left) {
    nodePop();
  }
  expectOp("[");
  parseExpressionableRoot(hs);
  expectSym(']');
  struct node *expNode = nodePop();
  makeBracketNode(expNode);
  if (left) {
    struct node *bracketNode = nodePop();
    makeExpNode(left, bracketNode, "[]");
  }
}
void parseCast() {
  struct datatype dtype = {};
  parseDatatype(&dtype);
  expectSym(')');
  parseExpressionable(historyBegin(0));
  struct node *opperandNode = nodePop();
  makeCastNode(&dtype, opperandNode);
}
int parseExp(history *hs) {
  if (S_EQ(tokenPeekNext()->sval, "(")) {
    parseForParentheses(hs);
    return 0;
  } else if (S_EQ(tokenPeekNext()->sval, "?")) {
    parseTenary(hs);
    return 0;

  } else if (S_EQ(tokenPeekNext()->sval, "[")) {
    parseForArray(hs);
    return 0;
  } else if (S_EQ(tokenPeekNext()->sval, ",")) {
    parseForComma(hs);
    return 0;
  } else {
    int res = parseExpressionNormal(hs);
    return res;
  }
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
size_t sizeOfStruct(const char *structName) {
  struct symbol *sym = symresolverGetSymbol(currentProcess, structName);
  if (!sym) {
    return 0;
  }
  if (sym->type != SYMBOL_TYPE_NODE) {
    compilerError(currentProcess, "Symbol is not a node \n");
  }
  struct node *node = sym->data;
  if (node->type != NODE_TYPE_STRUCT) {
    compilerError(currentProcess, "node is not a struct \n");
  }
  return node->_struct.body_n->body.size;
}
size_t sizeOfUnion(const char *unionName) {
  struct symbol *sym = symresolverGetSymbol(currentProcess, unionName);
  if (!sym) {
    return 0;
  }
  if (sym->type != SYMBOL_TYPE_NODE) {
    compilerError(currentProcess, "Symbol is not a node \n");
  }
  struct node *node = sym->data;
  if (node->type != NODE_TYPE_UNION) {
    compilerError(currentProcess, "node is not a struct \n");
  }
  return node->_union.body_n->body.size;
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
    typeOut->type = DATA_TYPE_STRUCT;
    typeOut->size = sizeOfStruct(datatypeToken->sval);
    typeOut->structNode =
        structNodeForName(currentProcess, datatypeToken->sval);
    break;
  case DATA_TYPE_EXPECT_UNION:
    typeOut->type = DATA_TYPE_UNION;
    typeOut->size = sizeOfUnion(datatypeToken->sval);
    typeOut->unionNode = unionNodeForName(currentProcess, datatypeToken->sval);
    break;
  default:
    compilerError(currentProcess, "Unsupported datatype expectation \n");
  }

  if (ptrDepth > 0) {
    typeOut->flags |= DATATYPE_FLAG_IS_POINTER;
    typeOut->ptrDepth = ptrDepth;
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
struct datatypeStructNodeFixPrivate {
  struct node *node;
};
bool datatypeStructNodeFix(struct fixup *fixup) {
  struct datatypeStructNodeFixPrivate *privateF = fixupPrivate(fixup);
  struct datatype *dtype = &privateF->node->var.type;
  dtype->type = DATA_TYPE_STRUCT;
  dtype->size = sizeOfStruct(dtype->typeStr);
  dtype->structNode = structNodeForName(currentProcess, dtype->typeStr);
  if (!dtype->structNode) {
    return false;
  }
  return true;
}
void datatypeStructNodeEnd(struct fixup *fixup) { free(fixupPrivate(fixup)); }
void makeVariableNode(datatype *dtype, token *name, node *valueNode) {
  const char *nameStr = NULL;
  if (name) {
    nameStr = name->sval;
  }

  nodeCreate(&(struct node){.type = NODE_TYPE_VARIABLE,
                            .var.name = nameStr,
                            .var.val = valueNode,
                            .var.type = *dtype});
  struct node *varNode = nodePeekOrNull();
  if (varNode->var.type.type == DATA_TYPE_STRUCT &&
      !varNode->var.type.structNode) {
    struct datatypeStructNodeFixPrivate *private =
        calloc(1, sizeof(struct datatypeStructNodeFixPrivate));
    vector_push(currentProcess->gb, &private);
    private->node = varNode;

    fixupRegister(parserFixupSystem,
                  &(struct fixupConfig){.fix = datatypeStructNodeFix,
                                        .end = datatypeStructNodeEnd,
                                        .privateData = private});
  }
}
static void expectKeyword(const char *keyword) {
  struct token *nextToken = tokenNext();
  if (!nextToken || nextToken->type != KEYWORD ||
      !S_EQ(nextToken->sval, keyword)) {
    compilerError(currentProcess,
                  "Expecting the keyword %s but something else was provided",
                  keyword);
  }
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

struct parserScopeEntity *parserScopeLastEntityStopAtGlobalScope() {
  return scopeLastEntityStopAt(currentProcess, currentProcess->scope.root);
}
bool isPrimitive(struct node *node) {
  if (node->type != NODE_TYPE_VARIABLE) {
    compilerError(currentProcess, "Not a variable");
  }
  return datatypeIsPrimitive(&node->var.type);
}
int parserScopeOffserForGlobal(struct node *node, history *hs) {
  // global vars have  offset
  return 0;
}
struct parserScopeEntity *parserScopeLastEntity() {
  return scopeLastEntity(currentProcess);
}
void parserScopeOffsetForStruct(node *node, history *hs) {
  int offset = 0;
  struct parserScopeEntity *lastEntity = parserScopeLastEntity();
  if (lastEntity) {
    offset += lastEntity->stackOffset + lastEntity->node->var.type.size;
    if (isPrimitive(node)) {
      node->var.padding = padding(offset, node->var.type.size);
    }
    node->var.aoffset = offset + node->var.padding;
  }
}

size_t functionNodeArgumentStackAddition(struct node *node) {
  if (node->type != NODE_TYPE_FUNCTION) {
    compilerError(currentProcess, "Not a function \n");
  }
  return node->func.args.stackAddition;
}
void parserScopeOffsetForStack(node *varNode, history *hs) {
  struct parserScopeEntity *lastEntity =
      parserScopeLastEntityStopAtGlobalScope();
  // for function arguments stack is upwards
  bool upwardStack = hs->flags & HISTORY_FLAG_IS_UPWARD_STACK;
  // minus because the stack grows downwards

  int offset = -variableSize(varNode);
  if (upwardStack) {
    size_t stackAddition =
        functionNodeArgumentStackAddition(parserCurrentFunction);
    offset = stackAddition;
    if (lastEntity) {
      offset = datatypeSize(&variableNode(lastEntity->node)->var.type);
    }
  }
  if (lastEntity) {
    offset += variableNode(lastEntity->node)->var.aoffset;
    if (isPrimitive(varNode)) {
      variableNode(varNode)->var.padding =
          padding(upwardStack ? offset : -offset, varNode->var.type.size);
    }
  }
  bool firstEntity = !lastEntity;
  if (nodeIsStructOrUnionVariable(varNode) &&
      variableStructOrUnionBodyNode(varNode)->body.padded) {
    variableNode(varNode)->var.padding =
        padding(upwardStack ? offset : -offset, DATA_SIZE_DWORD);
  }
  variableNode(varNode)->var.aoffset =
      offset + (upwardStack ? variableNode(varNode)->var.padding
                            : -variableNode(varNode)->var.padding);
}
void parserScopeOffset(node *varNode, history *hs) {
  if (hs->flags & HISTORY_FLAG_IS_GLOBAL_SCOPE) {
    parserScopeOffserForGlobal(varNode, hs);
    return;
  }
  if (hs->flags & HISTORY_FLAG_INSIDE_STRUCT) {
    parserScopeOffsetForStruct(varNode, hs);
    return;
  }
  parserScopeOffsetForStack(varNode, hs);
}
void parserScopePush(struct parserScopeEntity *entity, size_t size);
void makeVariableNodeAndRegister(history *hs, datatype *dtype, token *name,
                                 node *valueNode) {
  makeVariableNode(dtype, name, valueNode);
  node *varNode = nodePop();
  // cleanup
  parserScopeOffset(varNode, hs);
  parserScopePush(parserScopeEntityNew(varNode, varNode->var.aoffset, 0),
                  varNode->var.type.size);

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

void parseBody(size_t *variableSize, history *hs);
void parseFuctionBody(history *hs) {
  parseBody(NULL,
            historyDown(hs, hs->flags | HISTORY_FLAG_INSIDE_FUNCTION_BODY));
}
struct vector *parseFunctionArguments(history *hs);
void parseFunction(struct datatype *retType, struct token *nameToken,
                   history *hs) {
  struct vector *argumentsVector = NULL;
  parserScopeNew();
  makeFunctionNode(retType, nameToken->sval, NULL, NULL);

  struct node *functionNode = nodePeek();

  parserCurrentFunction = functionNode;
  if (dataTypeIsStructOrUnion(retType)) {
    functionNode->func.args.stackAddition += DATA_SIZE_DWORD;
  }
  expectOp("(");
  argumentsVector = parseFunctionArguments(historyBegin(0));
  expectSym(')');

  functionNode->func.args.vector = argumentsVector;
  if (symresolverGetSymbolForNativeFunction(currentProcess, nameToken->sval)) {
    functionNode->func.flags |= FUNCTION_NODE_FLAG_IS_NATIVE;
  }
  if (tokenNextIsSymbol('{')) {
    parseFuctionBody(historyBegin(0));
    struct node *bodyNode = nodePop();
    functionNode->func.bodyN = bodyNode;
  } else {
    expectSym(';');
  }

  parserCurrentFunction = NULL;
  parserScopeFinish(currentProcess);
}
void parseGoto(history *hs) {
  expectKeyword("goto");
  parseIdentifier(historyBegin(0));
  expectSym(';');

  struct node *labelNode = nodePop();
  makeGotoNode(labelNode);
}
void parseLabel(history *hs);
void parseSymbol() {
  if (tokenNextIsSymbol('{')) {
    size_t variableSize = 0;
    history *hs = historyBegin(HISTORY_FLAG_IS_GLOBAL_SCOPE);
    parseBody(&variableSize, hs);
    struct node *bodyNode = nodePop();
    nodePush(bodyNode);
  } else if (tokenNextIsSymbol(':')) {
    parseLabel(historyBegin(0));
    return;
  }
  compilerError(currentProcess, "Invalid symbol was provided\n");
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

void parserScopePush(struct parserScopeEntity *entity, size_t size) {
  scopePush(currentProcess, entity, size);
}

node *variableStructOrUnionBodyNode(struct node *node) {
  if (!nodeIsStructOrUnionVariable(node)) {
    return NULL;
  }
  if (node->var.type.type == DATA_TYPE_STRUCT) {
    return node->var.type.structNode->_struct.body_n;
  }
  if (node->var.type.type == DATA_TYPE_UNION) {
    return node->var.type.unionNode->_union.body_n;
  }
  return NULL;
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

void parseBodyMultipleStatements(size_t *varSize, struct vector *bodyVec,
                                 history *hs) {
  // make body node
  makeBodyNode(NULL, 0, false, NULL);
  struct node *bodyNode = nodePop();
  bodyNode->binded.owner = parserCurrentBody;
  parserCurrentBody = bodyNode;

  struct node *stmtNode = NULL;
  struct node *largestPossibleVarNode = NULL;
  struct node *largestAlignEligibleNode = NULL;

  expectSym('{');

  while (!tokenNextIsSymbol('}')) {
    parseStatement(historyDown(hs, hs->flags));
    stmtNode = nodePop();
    if (stmtNode->type == NODE_TYPE_VARIABLE) {
      if (!largestPossibleVarNode ||
          (largestPossibleVarNode->var.type.size <= stmtNode->var.type.size)) {
        largestPossibleVarNode = stmtNode;
      }
      if (isPrimitive(stmtNode)) {
        if (!largestAlignEligibleNode ||
            (largestAlignEligibleNode->var.type.size <=
             stmtNode->var.type.size)) {
          largestAlignEligibleNode = stmtNode;
        }
      }
    }

    vector_push(bodyVec, &stmtNode);
    parserAppendSizeForNode(hs, varSize, variableNodeOrList(stmtNode));
  }

  expectSym('}');
  parserFinaliseBody(hs, bodyNode, bodyVec, varSize, largestAlignEligibleNode,
                     largestPossibleVarNode);
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
  vector_push(currentProcess->gbForVectors, &bodyVec);
  if (!tokenNextIsSymbol('{')) {
    parseBodySingleStatement(variableSize, bodyVec, hs);
    parserScopeFinish(currentProcess);
    return;
  }
  parseBodyMultipleStatements(variableSize, bodyVec, hs);
  parserScopeFinish(currentProcess);
  if (variableSize) {
    if (hs->flags & HISTORY_FLAG_INSIDE_FUNCTION_BODY) {
      parserCurrentFunction->func.stackSize += *variableSize;
    }
  }
}

void parseStructNoNewScope(datatype *type, bool isForwardDecleration) {
  struct node *bodyNode = NULL;
  size_t bodyVarSize = 0;
  if (!isForwardDecleration) {
    parseBody(&bodyVarSize, historyBegin(HISTORY_FLAG_INSIDE_STRUCT));
    bodyNode = nodePop();
  }
  makeStructNode(type->typeStr, bodyNode);
  struct node *structNode = nodePop();
  if (bodyNode) {
    type->size = bodyNode->body.size;
  }
  type->structNode = structNode;
  if (tokenIsIdentifier(tokenPeekNext())) {
    token *varName = tokenNext();
    structNode->flags |= NODE_FLAG_HAS_VARIABLE_COMBINED;
    if (type->flags & DATATYPE_FLAG_STRUCT_UNION_NO_NAME) {
      type->typeStr = varName->sval;
      type->flags &= ~DATATYPE_FLAG_STRUCT_UNION_NO_NAME;
      structNode->_struct.name = varName->sval;
    }

    makeVariableNodeAndRegister(historyBegin(0), type, varName, NULL);
    structNode->_struct.var = nodePop();
  }
  expectSym(';');
  nodePush(structNode);
}
void parseUnionNoScope(struct datatype *dtype, bool isForwardDecleration) {
  struct node *bodyNode = NULL;
  size_t bodyVariableSize = 0;
  if (!isForwardDecleration) {
    parseBody(&bodyVariableSize, historyBegin(HISTORY_FLAG_INSIDE_UNION));
    bodyNode = nodePop();
  }
  makeUnionNode(dtype->typeStr, bodyNode);
  struct node *unionNode = nodePop();
  if (bodyNode) {
    dtype->size = bodyNode->body.size;
  }
  if (tokenPeekNext()->type == IDENTIFIER) {
    token *varName = tokenNext();
    unionNode->flags |= NODE_FLAG_HAS_VARIABLE_COMBINED;
    makeVariableNodeAndRegister(historyBegin(0), dtype, varName, NULL);
    unionNode->_union.var = nodePop();
  }
  expectSym(';');
  nodePush(unionNode);
}
void parseUnion(struct datatype *dtype) {
  bool isForwardDecleration = !tokenIsSymbol(tokenPeekNext(), '{');
  if (!isForwardDecleration) {
    parserScopeNew();
  }
  parseUnionNoScope(dtype, isForwardDecleration);
  if (!isForwardDecleration) {
    parserScopeFinish(currentProcess);
  }
}
void parseStruct(struct datatype *dtype) {
  bool isForwardDecleration = !tokenIsSymbol(tokenPeekNext(), '{');
  if (!isForwardDecleration) {
    parserScopeNew();
  }
  parseStructNoNewScope(dtype, isForwardDecleration);
  if (!isForwardDecleration) {
    parserScopeFinish(currentProcess);
  }
}
void parseStructOrUnion(struct datatype *dtype) {
  switch (dtype->type) {
  case DATA_TYPE_STRUCT:
    parseStruct(dtype);
    break;
  case DATA_TYPE_UNION:
    parseUnion(dtype);
    break;
  default:
    compilerError(currentProcess,
                  "The provided data type is not a struct or a union\n");
  }
}
void tokenReadDots(size_t amount) {
  for (size_t i = 0; i < amount; i++) {
    expectOp(".");
  }
}
void parseVariableFull(history *hs) {
  struct datatype dtype;
  parseDatatype(&dtype);
  struct token *namedToken = NULL;
  if (tokenPeekNext()->type == IDENTIFIER) {
    namedToken = tokenNext();
  }
  parseVariable(&dtype, namedToken, hs);
}
struct vector *parseFunctionArguments(history *hs) {
  parserScopeNew();
  struct vector *argumentsVector = vector_create(sizeof(struct node *));
  vector_push(currentProcess->gbForVectors, &argumentsVector);
  while (!tokenNextIsSymbol(')')) {
    if (tokenNextIsOperator(".")) {
      tokenReadDots(3);
      parserScopeFinish(currentProcess);
      return argumentsVector;
    }
    parseVariableFull(
        historyDown(hs, hs->flags | HISTORY_FLAG_IS_UPWARD_STACK));
    struct node *argumentNode = nodePop();
    vector_push(argumentsVector, &argumentNode);
    if (!tokenNextIsOperator(",")) {
      break;
    }
    tokenNext();
  }
  parserScopeFinish(currentProcess);
  return argumentsVector;
}
void parseForwardDecleration(struct datatype *dtype) { parseStruct(dtype); }
void parseVariableFunctionStructUnion(struct history *hs) {
  datatype dtype;
  parseDatatype(&dtype);

  if (dataTypeIsStructOrUnion(&dtype) && tokenNextIsSymbol('{')) {
    parseStructOrUnion(&dtype);
    struct node *suNode = nodePop();
    symresolverBuildForNode(currentProcess, suNode);
    nodePush(suNode);
    return;
  }

  // forward decleration
  if (tokenNextIsSymbol(';')) {
    parseForwardDecleration(&dtype);
    return;
  }

  parserIgnoreInt(&dtype);
  token *namedToken = tokenNext();
  if ((namedToken && namedToken->type != IDENTIFIER) || (!namedToken)) {
    compilerError(currentProcess,
                  "Expecting a valid name after variable decleration\n");
  }
  if (tokenNextIsOperator("(")) {
    parseFunction(&dtype, namedToken, hs);
    return;
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
struct node *parseElse(history *hs) {
  size_t varSize = 0;
  parseBody(&varSize, hs);
  struct node *bodyNode = nodePop();
  makeElseNode(bodyNode);
  return nodePop();
}
void parseIfStmt(history *hs);
struct node *parseElseOrElseIf(history *hs) {
  struct node *node = NULL;
  if (tokenNextIsKeyword("else")) {
    tokenNext();
    if (tokenNextIsKeyword("if")) {
      parseIfStmt(historyDown(hs, 0));
      node = nodePop();
      return node;
    }
  }

  return node;
  ;
}
void parseIfStmt(history *hs) {
  expectKeyword("if");
  expectOp("(");
  parseExpressionableRoot(hs);
  expectSym(')');
  struct node *condNode = nodePop();
  size_t varSize = 0;
  parseBody(&varSize, hs);
  struct node *bodyNode = nodePop();
  makeIfNode(condNode, bodyNode, parseElseOrElseIf(hs));
}
void parseKeywordParenthesisExpression(const char *keyword) {
  expectKeyword(keyword);
  expectOp("(");
  parseExpressionableRoot(historyBegin(0));
  expectSym(')');
}
void parseCase(history *hs) {
  expectKeyword("case");
  parseExpressionableRoot(hs);
  struct node *caseExpNode = nodePop();
  expectSym(':');
  makeCaseNode(caseExpNode);
  if (caseExpNode->type != NODE_TYPE_NUMBER) {
    compilerError(currentProcess, "We only support numeric cases \n");
  }
  struct node *caseNode = nodePop();
  parserRegisterCase(hs, caseNode);
}
void parseSwitch(history *hs) {
  struct parserHistorySwitch _switch = parserNewSwitchStmt(hs);
  parseKeywordParenthesisExpression("switch");
  struct node *switchExpNode = nodePop();
  size_t varSize = 0;
  parseBody(&varSize, hs);
  struct node *bodyNode = nodePop();
  makeSwitchNode(switchExpNode, bodyNode, _switch.caseData.cases,
                 _switch.caseData.hasDefaultCase);
}
void parseDoWhile(history *hs) {
  expectKeyword("do");
  size_t varSize = 0;
  parseBody(&varSize, hs);
  struct node *bodyNode = nodePop();
  parseKeywordParenthesisExpression("while");
  struct node *expNode = nodePop();
  expectSym(';');
  makeDoWhileNode(bodyNode, expNode);
}
void parseWhile(history *hs) {
  parseKeywordParenthesisExpression("while");
  struct node *expNode = nodePop();
  size_t varSize = 0;
  parseBody(&varSize, hs);
  struct node *bodyNode = nodePop();
  makeWhileNode(expNode, bodyNode);
}
bool parseForLoopPart(history *hs) {
  if (tokenNextIsSymbol(';')) {
    tokenNext();
    return false;
  }
  parseExpressionableRoot(hs);
  expectSym(';');
  return true;
}
bool parseForLoopPartLoop(history *hs) {
  if (tokenNextIsSymbol(')')) {
    return false;
  }
  parseExpressionableRoot(hs);
  return true;
}
void parseForStmt(history *hs) {
  struct node *initNode = NULL;
  struct node *condNode = NULL;
  struct node *loopNode = NULL;
  struct node *bodyNode = NULL;
  expectKeyword("for");
  expectOp("(");
  if (parseForLoopPart(hs)) {
    initNode = nodePop();
  }
  if (parseForLoopPart(hs)) {
    condNode = nodePop();
  }
  if (parseForLoopPartLoop(hs)) {
    loopNode = nodePop();
  }
  expectSym(')');

  size_t variableSize = 0;
  parseBody(&variableSize, hs);
  bodyNode = nodePop();

  makeForNode(initNode, condNode, loopNode, bodyNode);
}
void parseReturn(history *hs) {
  expectKeyword("return");
  if (tokenNextIsSymbol(';')) {
    expectSym(';');
    makeReturnNode(NULL);
    return;
  }
  parseExpressionableRoot(hs);
  struct node *expressionNode = nodePop();
  makeReturnNode(expressionNode);
  expectSym(';');
}
void parseLabel(history *hs) {
  expectSym(':');
  struct node *labelNameNode = nodePop();
  if (labelNameNode->type != NODE_TYPE_IDENTIFIER) {
    compilerError(
        currentProcess,
        "Expecting identifier for labels. Something else was provided \n");
  }
  makeLabelNode(labelNameNode);
}
void parseContinue(history *hs) {
  expectKeyword("continue");
  expectSym(';');
  makeContinueNode();
}
void parseBreak(history *hs) {
  expectKeyword("break");
  expectSym(';');
  makeBreakNode();
}
void parseTenary(history *hs) {
  struct node *conditionNode = nodePop();
  expectOp("?");
  parseExpressionableRoot(
      historyDown(hs, HISTORY_FLAG_PARENTHESES_NOT_A_FUNCTION_CALL));
  struct node *trueResultNode = nodePop();
  expectSym(':');
  parseExpressionableRoot(
      historyDown(hs, HISTORY_FLAG_PARENTHESES_NOT_A_FUNCTION_CALL));
  struct node *falseResultNode = nodePop();
  makeTenaryNode(trueResultNode, falseResultNode);
  struct node *tenaryNode = nodePop();
  makeExpNode(conditionNode, tenaryNode, "?");
}
int parseKeyword(struct history *hs) {
  token *token = tokenPeekNext();
  if (isKeywordVariableModifier(token->sval) ||
      keywordIsDataType(token->sval)) {
    parseVariableFunctionStructUnion(hs);
    return 0;
  }
  if (S_EQ(token->sval, "return")) {
    parseReturn(hs);
    return 0;
  }
  if (S_EQ(token->sval, "if")) {
    parseIfStmt(hs);
    return 0;
  }
  if (S_EQ(token->sval, "for")) {
    parseForStmt(hs);
    return 0;
  }
  if (S_EQ(token->sval, "while")) {
    parseWhile(hs);
    return 0;
  }
  if (S_EQ(token->sval, "do")) {
    parseDoWhile(hs);
    return 0;
  }
  if (S_EQ(token->sval, "switch")) {
    parseSwitch(hs);
    return 0;
  }
  if (S_EQ(token->sval, "continue")) {
    parseContinue(hs);
    return 0;
  }
  if (S_EQ(token->sval, "break")) {
    parseBreak(hs);
    return 0;
  }
  if (S_EQ(token->sval, "goto")) {
    parseGoto(hs);
    return 0;
  }
  if (S_EQ(token->sval, "case")) {
    parseCase(hs);
    return 0;
  }
  // we ll see, maybe free it with garbage vec
  compilerError(currentProcess, "invalid keyword");
  return -1;
}
void parseString(history *hs) { parseSingleTokenToNode(); }
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
    res = 0;
    break;
  case OPERATOR:
    parseExp(hs);
    res = 0;
    break;
  case KEYWORD:
    parseKeyword(hs);
    res = 0;
    break;
  case STRING:
    parseString(hs);
    res = 0;
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
    res = 0;
    break;
  case KEYWORD:
    parseKeywordForGlobal();
    res = 0;
    break;
  case SYMBOL:
    parseSymbol();
    res = 0;
    break;
  }
  return res;
}

int parse(compileProcess *process) {

  createRootScope(process);
  currentProcess = process;
  parserLastToken = NULL;
  nodeSetVector(process->nodeVec, process->nodeTreeVec, process->nodeGarbageVec,
                process->gbForVectors);
  parserBlancNode = nodeCreate(&(struct node){.type = NODE_TYPE_BLANK});
  parserFixupSystem = fixupSystemNew();
  currentProcess->parserFixupSystem = parserFixupSystem;
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
  if (!fixupsResolve(parserFixupSystem)) {
    compilerError(currentProcess, "There is an unresolved reference \n");
  }
  // TODO: maybe free root scope
  return PARSE_ALL_OK;
}
