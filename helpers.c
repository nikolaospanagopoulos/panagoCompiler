#include "compileProcess.h"
#include "compiler.h"
#include "node.h"
#include "position.h"
#include "vector.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
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
bool isLogicalOperator(const char *op) {
  return S_EQ(op, "&&") || S_EQ(op, "||");
}

bool isLogicalNode(struct node *node) {
  return node->type == NODE_TYPE_EXPRESSION && isLogicalOperator(node->exp.op);
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

struct datatype *datatypeThatsAPointer(struct datatype *d1,
                                       struct datatype *d2) {
  if (d1->flags & DATATYPE_FLAG_IS_POINTER) {
    return d1;
  }
  if (d2->flags & DATATYPE_FLAG_IS_POINTER) {
    return d2;
  }
  return NULL;
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
struct node *bodyLargestVarNode(struct node *bodyNode) {
  if (!bodyNode) {
    return NULL;
  }
  if (bodyNode->type != NODE_TYPE_BODY) {
    return NULL;
  }
  return bodyNode->body.largestVarNode;
}
struct node *variableStructOrUnionLargestVarNode(struct node *varNode) {
  return bodyLargestVarNode(variableStructOrUnionBodyNode(varNode));
}
int structOffset(struct compileProcess *process, const char *structName,
                 const char *varName, struct node **varNodeOut, int lastPos,
                 int flags) {
  struct symbol *structSym = symresolverGetSymbol(process, structName);
  if (!structSym || structSym->type != SYMBOL_TYPE_NODE) {
    compilerError(process, "Symbol doesnt exist \n");
  }
  struct node *node = structSym->data;
  if (!nodeIsStructOrUnion(node)) {
    compilerError(process, "Not a struct or a union \n");
  }
  struct vector *structVarsVec = node->_struct.body_n->body.statements;
  vector_set_peek_pointer(structVarsVec, 0);
  if (flags & STRUCT_ACCESS_BACKWARDS) {
    vector_set_peek_pointer_end(structVarsVec);
    vector_set_flag(structVarsVec, VECTOR_FLAG_PEEK_DECREMENT);
  }
  struct node *varNodeCur = variableNode(vector_peek_ptr(structVarsVec));
  struct node *varNodeLast = NULL;
  int position = lastPos;
  *varNodeOut = NULL;
  while (varNodeCur) {
    *varNodeOut = varNodeCur;
    if (varNodeLast) {
      position += variableSize(varNodeLast);
      if (isPrimitive(varNodeCur)) {
        position = alignValueAsPositive(position, varNodeCur->var.type.size);
      } else {
        position = alignValueAsPositive(
            position,
            variableStructOrUnionLargestVarNode(varNodeCur)->var.type.size);
      }
    }

    if (S_EQ(varNodeCur->var.name, varName)) {
      break;
    }
    varNodeLast = varNodeCur;
    varNodeCur = variableNode(vector_peek_ptr(structVarsVec));
  }
  vector_unset_flag(structVarsVec, VECTOR_FLAG_PEEK_DECREMENT);
  return position;
}
bool isAccessOperator(const char *op) {
  return S_EQ(op, "->") || S_EQ(op, ".");
}
bool isAccessNode(struct node *node) {
  return node->type == NODE_TYPE_EXPRESSION && isAccessOperator(node->exp.op);
}
bool isParenthesesOperator(const char *op) { return S_EQ(op, "()"); }

bool isParenthesesNode(struct node *node) {
  return node->type == NODE_TYPE_EXPRESSION &&
         isParenthesesOperator(node->exp.op);
}
bool isAccessNodeWithOp(struct node *node, const char *op) {
  return isAccessNode(node) && S_EQ(node->exp.op, op);
}
bool isArgumentOperator(const char *op) { return S_EQ(op, ","); }

bool isArgumentNode(struct node *node) {
  return node->type == NODE_TYPE_EXPRESSION && isArgumentOperator(node->exp.op);
}

void datatypeDecrementPtr(struct datatype *dtype) {
  dtype->ptrDepth--;
  if (dtype->ptrDepth <= 0) {
    // unset is ptr flag
    dtype->flags &= ~DATATYPE_FLAG_IS_POINTER;
  }
}

bool isUnaryOperator(const char *op) {
  return S_EQ(op, "-") || S_EQ(op, "!") || S_EQ(op, "~") || S_EQ(op, "*") ||
         S_EQ(op, "&");
}
bool opIsIndirection(const char *op) { return S_EQ(op, "*"); }

bool opIsAddress(const char *op) { return S_EQ(op, "&"); }

struct datatype datatypeForNumeric() {
  struct datatype dtype = {};
  dtype.flags |= DATATYPE_FLAG_IS_LITERAL;
  dtype.type = DATA_TYPE_INTEGER;
  dtype.typeStr = "int";
  dtype.size = DATA_SIZE_DWORD;
  return dtype;
}
