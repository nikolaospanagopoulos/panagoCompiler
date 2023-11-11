#include "node.h"
#include "compileProcess.h"
#include "compiler.h"
#include "vector.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct vector *nodeVector = NULL;
struct vector *nodeVectorRoot = NULL;
struct vector *garbage = NULL;
struct vector *garbageForVector = NULL;
struct node *parserCurrentBody = NULL;
struct node *parserCurrentFunction = NULL;

void nodeSetVector(struct vector *vec, struct vector *rootVec,
                   struct vector *garbageVec, struct vector *garbageForVec) {
  nodeVector = vec;
  nodeVectorRoot = rootVec;
  garbage = garbageVec;
  garbageForVector = garbageForVec;
}

void nodePush(node *node) { vector_push(nodeVector, &node); }
void garpush(node *node) { vector_push(garbage, &node); }

node *nodePeekOrNull() { return vector_back_ptr_or_null(nodeVector); }

node *nodePeek() { return *(struct node **)(vector_back(nodeVector)); }

node *nodePop() {
  node *lastNode = vector_back_ptr_or_null(nodeVector);
  node *lastNodeRoot =
      vector_empty(nodeVector) ? NULL : vector_back_ptr_or_null(nodeVectorRoot);

  vector_pop(nodeVector);

  if (lastNode == lastNodeRoot) {
    vector_pop(nodeVectorRoot);
  }

  return lastNode;
}
bool nodeIsExpressionable(node *node) {
  return node->type == NODE_TYPE_EXPRESSION ||
         node->type == NODE_TYPE_EXPRESSION_PARENTHESES ||
         node->type == NODE_TYPE_UNARY || node->type == NODE_TYPE_IDENTIFIER ||
         node->type == NODE_TYPE_NUMBER || node->type == NODE_TYPE_STRING;
}
node *nodeCreate(node *_node) {
  node *node = malloc(sizeof(struct node));
  memcpy(node, _node, sizeof(struct node));
  node->binded.owner = parserCurrentBody;
  node->binded.function = parserCurrentFunction;
  garpush(node);
  nodePush(node);

  return node;
}
node *nodeCreateNotPush(node *_node) {
  node *node = malloc(sizeof(struct node));
  memcpy(node, _node, sizeof(struct node));
  node->varlist.list = NULL;
  return node;
}
node *nodePeekExpressionableOrNull() {
  node *lastNode = nodePeekOrNull();
  return nodeIsExpressionable(lastNode) ? lastNode : NULL;
}

node *makeExpNode(node *left, node *right, const char *op) {
  assert(left);
  assert(right);

  node *tocreate = nodeCreate(&(node){.type = NODE_TYPE_EXPRESSION,
                                      .exp.left = left,
                                      .exp.right = right,
                                      .exp.op = op});

  return tocreate;
}
void makeBracketNode(node *node) {
  nodeCreate(&(struct node){.type = NODE_TYPE_BRACKET, .bracket.inner = node});
}
void makeBodyNode(struct vector *bodyVec, size_t size, bool padded,
                  struct node *largestVarNode) {
  nodeCreate(&(struct node){.type = NODE_TYPE_BODY,
                            .body.statements = bodyVec,
                            .body.size = size,
                            .body.padded = padded,
                            .body.largestVarNode = largestVarNode});
}
bool nodeIsStructOrUnionVariable(struct node *node) {
  if (node->type != NODE_TYPE_VARIABLE) {
    return false;
  }
  return dataTypeIsStructOrUnion(&node->var.type);
}
struct node *variableNode(struct node *node) {
  struct node *varNode = NULL;
  switch (node->type) {
  case NODE_TYPE_VARIABLE:
    varNode = node;
    break;
  case NODE_TYPE_STRUCT:
    varNode = node->_struct.var;
    break;
  case NODE_TYPE_UNION:
    varNode = node->_union.var;
    break;
  }
  return varNode;
}

node *variableNodeOrList(struct node *node) {
  if (node->type == NODE_TYPE_VARIABLE_LIST) {
    return node;
  }
  return variableNode(node);
}

void makeStructNode(const char *name, struct node *bodyNode) {
  int flags = 0;
  if (!bodyNode) {
    flags |= NODE_FLAG_IS_FORWARD_DECLERATION;
  }
  nodeCreate(&(struct node){.type = NODE_TYPE_STRUCT,
                            ._struct.body_n = bodyNode,
                            ._struct.name = name,
                            .flags = flags});
}
void makeUnionNode(const char *name, struct node *bodyNode) {
  int flags = 0;
  if (!bodyNode) {
    flags |= NODE_FLAG_IS_FORWARD_DECLERATION;
  }
  nodeCreate(&(struct node){.type = NODE_TYPE_UNION,
                            ._union.body_n = bodyNode,
                            ._union.name = name,
                            .flags = flags});
}
struct node *nodeFromSym(struct symbol *sym) {
  if (sym->type != SYMBOL_TYPE_NODE) {
    return NULL;
  }
  return sym->data;
}
struct node *nodeFromSymbol(compileProcess *process, const char *name) {
  struct symbol *sym = symresolverGetSymbol(process, name);
  if (!sym) {
    return NULL;
  }
  return nodeFromSym(sym);
}
struct node *structNodeForName(compileProcess *process, const char *name) {
  struct node *node = nodeFromSymbol(process, name);
  if (!node) {
    return NULL;
  }
  if (node->type != NODE_TYPE_STRUCT) {
    return NULL;
  }
  return node;
}
struct node *unionNodeForName(compileProcess *process, const char *name) {
  struct node *node = nodeFromSymbol(process, name);
  if (!node) {
    return NULL;
  }
  if (node->type != NODE_TYPE_UNION) {
    return NULL;
  }
  return node;
}

void makeExpParenthesisNode(struct node *expNode) {
  nodeCreate(
      &(struct node){.type = NODE_TYPE_EXPRESSION, .parenthesis.exp = expNode});
}
void makeFunctionNode(struct datatype *retType, const char *name,
                      struct vector *arguments, struct node *bodyNode) {
  struct node *funcNode =
      nodeCreate(&(struct node){.type = NODE_TYPE_FUNCTION,
                                .func.name = name,
                                .func.args.vector = arguments,
                                .func.bodyN = bodyNode,
                                .func.rtype = *retType,
                                .func.args.stackAddition = DATA_SIZE_DDWORD});

  funcNode->func.frame.elements =
      vector_create(sizeof(struct stackFrameElement));

  if (arguments) {
    vector_push(garbageForVector, &arguments);
  }
  // collect garbage
  vector_push(garbageForVector, &funcNode->func.frame.elements);
}
bool nodeIsExpressionOrParentheses(struct node *node) {
  return node->type == NODE_TYPE_EXPRESSION_PARENTHESES ||
         node->type == NODE_TYPE_EXPRESSION;
}
bool nodeIsValueType(struct node *node) {
  return nodeIsExpressionOrParentheses(node) ||
         node->type == NODE_TYPE_IDENTIFIER || node->type == NODE_TYPE_NUMBER ||
         node->type == NODE_TYPE_UNARY || node->type == NODE_TYPE_TENARY ||
         node->type == NODE_TYPE_STRING;
}
void makeIfNode(struct node *condNode, struct node *bodyNode,
                struct node *nextNode) {
  nodeCreate(&(struct node){.type = NODE_TYPE_STATEMENT_IF,
                            .stmt.ifStmt.condNode = condNode,
                            .stmt.ifStmt.bodyNode = bodyNode,
                            .stmt.ifStmt.next = nextNode});
}
void makeElseNode(struct node *bodyNode) {
  nodeCreate(&(struct node){.type = NODE_TYPE_STATEMENT_ELSE,
                            .stmt.ifStmt.bodyNode = bodyNode});
}

void makeReturnNode(struct node *expNode) {
  nodeCreate(&(struct node){.type = NODE_TYPE_STATEMENT_RETURN,
                            .stmt.returnStmt.expression = expNode});
}
void makeForNode(struct node *initNode, struct node *condNode,
                 struct node *loopNode, struct node *bodyNode) {
  nodeCreate(&(struct node){.type = NODE_TYPE_STATEMENT_FOR,
                            .stmt.forStmt.initNode = initNode,
                            .stmt.forStmt.loopNode = loopNode,
                            .stmt.forStmt.bodyNode = bodyNode});
}
void makeWhileNode(struct node *expNode, struct node *bodyNode) {
  nodeCreate(&(struct node){.type = NODE_TYPE_STATEMENT_WHILE,
                            .stmt.whileStmt.expNode = expNode,
                            .stmt.whileStmt.bodyNode = bodyNode});
}
void makeDoWhileNode(struct node *bodyNode, struct node *expNode) {
  nodeCreate(&(struct node){.type = NODE_TYPE_STATEMENT_DO_WHILE,
                            .stmt.whileStmt.expNode = expNode,
                            .stmt.whileStmt.bodyNode = bodyNode});
}
void makeSwitchNode(struct node *expNode, struct node *bodyNode,
                    struct vector *cases, bool hasDefaultCase) {
  nodeCreate(&(struct node){.type = NODE_TYPE_STATEMENT_SWITCH,
                            .stmt.switchStmt.exp = expNode,
                            .stmt.switchStmt.body = bodyNode,
                            .stmt.switchStmt.cases = cases,
                            .stmt.switchStmt.hasDefaultCase = hasDefaultCase});
}
void makeContinueNode() {
  nodeCreate(&(struct node){.type = NODE_TYPE_STATEMENT_CONTINUE});
}
void makeBreakNode() {
  nodeCreate(&(struct node){.type = NODE_TYPE_STATEMENT_BREAK});
}
void makeLabelNode(struct node *labelNode) {
  nodeCreate(
      &(struct node){.type = NODE_TYPE_LABEL, .nodeLabel.name = labelNode});
}
void makeGotoNode(struct node *labelNode) {
  nodeCreate(&(struct node){.type = NODE_TYPE_STATEMENT_GOTO,
                            .stmt.gotoStmt.labelNode = labelNode});
}
void makeCaseNode(struct node *labelNode) {
  nodeCreate(&(struct node){.type = NODE_TYPE_STATEMENT_CASE,
                            .stmt._case.exp = labelNode});
}
void makeTenaryNode(struct node *trueNode, struct node *falseNode) {
  nodeCreate(&(struct node){.type = NODE_TYPE_TENARY,
                            .tenary.trueNode = trueNode,
                            .tenary.falseNode = falseNode});
}
void makeCastNode(struct datatype *dtype, struct node *opperandNode) {
  nodeCreate(&(struct node){.type = NODE_TYPE_CAST,
                            .cast.dtype = *dtype,
                            .cast.operand = opperandNode});
}
bool nodeIsExpression(struct node *node, const char *op) {
  return node->type == NODE_TYPE_EXPRESSION && S_EQ(node->exp.op, op);
}
bool isArrayNode(struct node *node) { return nodeIsExpression(node, "[]"); }
bool isNodeAssignment(struct node *node) {
  if (node->type != NODE_TYPE_EXPRESSION) {
    return false;
  }

  return S_EQ(node->exp.op, "=") || S_EQ(node->exp.op, "+=") ||
         S_EQ(node->exp.op, "-=") || S_EQ(node->exp.op, "/=") ||
         S_EQ(node->exp.op, "*=");
}
bool nodeIsStructOrUnion(struct node *node) {
  return node->type == NODE_TYPE_STRUCT || node->type == NODE_TYPE_UNION;
}

bool nodeValid(struct node *node) {
  return node && node->type != NODE_TYPE_BLANK;
}
void makeUnaryNode(const char *op, struct node *operandNode) {
  nodeCreate(&(struct node){
      .type = NODE_TYPE_UNARY, .unary.op = op, .unary.operand = operandNode});
}
