#include "node.h"
#include "compiler.h"
#include "vector.h"
#include <stdlib.h>
#include <string.h>

struct vector *nodeVector = NULL;
struct vector *nodeVectorRoot = NULL;

void nodeSetVector(struct vector *vec, struct vector *rootVec) {
  nodeVector = vec;
  nodeVectorRoot = rootVec;
}

void nodePush(node *node) { vector_push(nodeVector, &node); }

node *nodePeekOrNull() { return vector_back_or_null(nodeVector); }

node *nodePeek() { return *(struct node **)(vector_back(nodeVector)); }

node *nodePop() {
  node *lastNode = vector_back_ptr(nodeVector);
  node *lastNodeRoot =
      vector_empty(nodeVector) ? NULL : vector_back_ptr(nodeVectorRoot);

  vector_pop(nodeVector);

  if (lastNode == lastNodeRoot) {
    vector_pop(nodeVectorRoot);
  }
  return lastNode;
}

node *nodeCreate(node *_node) {
  node *node = malloc(sizeof(struct node));
  memcpy(node, _node, sizeof(struct node));

  nodePush(node);
  return node;
}
