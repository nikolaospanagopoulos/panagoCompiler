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
extern struct node *parserCurrentFunction;
struct node *parserBlancNode;
enum {
  PARSER_SCOPE_ENTITY_ON_STACK = 0b00000001,
  PARSER_SCOPE_ENTITY_STRUCTURE_SCOPE = 0b00000011

};

struct parserScopeEntity {
  int flags;
  int stackOffset;
  node *node;
};

static void expectSym(char c);
static void expectOp(const char *op);
void parserScopeNew();
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
  HISTORY_FLAG_INSIDE_FUNCTION_BODY = 0b00010000
};

typedef struct history {
  int flags;
} history;

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
void parserDealWithAdditionalExpression() {
  if (tokenPeekNext()->type == OPERATOR) {
    parseExpressionable(historyBegin(0));
  }
}
void parseForParentheses(history *hs) {
  expectOp("(");
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
int parseExp(history *hs) {
  if (S_EQ(tokenPeekNext()->sval, "(")) {
    parseForParentheses(hs);
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
    compilerError(currentProcess, "Cannot use unions yet \n");
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
void parseSymbol() {
  if (tokenNextIsSymbol('{')) {
    size_t variableSize = 0;
    history *hs = historyBegin(HISTORY_FLAG_IS_GLOBAL_SCOPE);
    parseBody(&variableSize, hs);
    struct node *bodyNode = nodePop();
    nodePush(bodyNode);
  }
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
  compilerWarning(currentProcess, "Union body not implemented yet \n");
  exit(1);
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
    res = 0;
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
