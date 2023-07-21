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
struct node *parserCurrentBody = NULL;

void nodeSetVector(struct vector *vec, struct vector *rootVec,
                   struct vector *garbageVec) {
  nodeVector = vec;
  nodeVectorRoot = rootVec;
  garbage = garbageVec;
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
    printf("unions are not yet supported\n");
    exit(-1);
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
