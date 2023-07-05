#include "node.h"
#include "compiler.h"
#include "vector.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct vector *nodeVector = NULL;
struct vector *nodeVectorRoot = NULL;
struct vector *garbage = NULL;

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
