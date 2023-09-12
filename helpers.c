#include "compileProcess.h"
#include "compiler.h"
#include "node.h"
#include "vector.h"
#include <stddef.h>
#include <stdlib.h>
static struct compileProcess *process;
static struct lexProcess *lexPr;

void setLexProcessForCompileProcessForHelpers(struct lexProcess *pr,
                                              struct compileProcess *cp) {
  lexPr = pr;
  process = cp;
}
size_t variableSize(struct node *varNode) {
  if (varNode->type != NODE_TYPE_VARIABLE) {
    compilerError(
        process,
        "Cannot calculate variableSize for a node that is not a variable\n");
  }
  return datatypeSize(&varNode->var.type);
}

size_t variableSizeForList(struct node *varListNode) {
  if (varListNode->type != NODE_TYPE_VARIABLE_LIST) {
    compilerError(
        process,
        "Cannot calculate variableSize for a node that is not a var list\n");
  }
  size_t size = 0;
  vector_set_peek_pointer(varListNode->varlist.list, 0);
  node *varNode = vector_peek_ptr(varListNode->varlist.list);
  while (varNode) {
    size += variableSize(varNode);
    varNode = vector_peek_ptr(varListNode->varlist.list);
  }
  return size;
}

int padding(int val, int to) {
  if (to <= 0) {
    return 0;
  }
  if ((val % to) == 0) {
    return 0;
  }
  return to - (val % to) % to;
}
int alignValue(int val, int to) {
  if (val % to) {
    val += padding(val, to);
  }
  return val;
}
int alignValueAsPositive(int val, int to) {
  if (to < 0) {
    compilerError(
        process,
        "Cannot calculate padding with a negative to function argument\n");
  }
  if (val < 0) {
    to = -to;
  }
  return alignValue(val, to);
}

int computeSumPadding(struct vector *vec) {
  int padding = 0;
  int lastType = 1;
  bool mixedTypes = false;
  vector_set_peek_pointer(vec, 0);
  node *curNode = vector_peek_ptr(vec);
  node *lastNode = NULL;
  while (curNode) {
    if (curNode->type != NODE_TYPE_VARIABLE) {
      curNode = vector_peek_ptr(vec);
      continue;
    }
    padding += curNode->var.padding;
    lastType = curNode->var.type.type;
    lastNode = curNode;
    curNode = vector_peek_ptr(vec);
  }
  return padding;
}
int arrayMultiplier(struct datatype *dtype, int index, int indexValue) {
  if (!(dtype->flags & DATATYPE_FLAG_IS_ARRAY)) {
    return indexValue;
  }
  vector_set_peek_pointer(dtype->array.brackets->nBrackets, index + 1);
  int sizeSum = indexValue;
  struct node *bracketNode = vector_peek_ptr(dtype->array.brackets->nBrackets);
  while (bracketNode) {
    if (bracketNode->bracket.inner->type != NODE_TYPE_NUMBER) {
      compilerError(process, "Not a number node \n");
    }
    int declaredIndex = bracketNode->bracket.inner->llnum;
    int sizeValue = declaredIndex;
    sizeSum *= sizeValue;
    bracketNode = vector_peek_ptr(dtype->array.brackets->nBrackets);
  }
  return sizeSum;
}
int arrayOffset(struct datatype *dtype, int index, int indexValue) {
  if (!(dtype->flags & DATATYPE_FLAG_IS_ARRAY) ||
      (index == vector_count(dtype->array.brackets->nBrackets) - 1)) {
    return indexValue * datatypeElementSize(dtype);
  }
  return arrayMultiplier(dtype, index, indexValue) * datatypeElementSize(dtype);
}
