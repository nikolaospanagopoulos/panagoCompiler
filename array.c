#include "compileProcess.h"
#include "compiler.h"
#include "node.h"
#include "vector.h"
#include <stddef.h>
#include <stdlib.h>
static struct compileProcess *cprocess;
static struct lexProcess *lexPr;
void setLexProcessCompileProcess(struct lexProcess *lpr,
                                 struct compileProcess *process) {
  lexPr = lpr;
  cprocess = process;
}
struct arrayBrackets *arrayBracketsNew() {
  struct arrayBrackets *brackets = calloc(1, sizeof(struct arrayBrackets));
  brackets->nBrackets = vector_create(sizeof(struct node *));
  return brackets;
}

void arrayBracketsAdd(struct arrayBrackets *brackets, node *bracketNode) {
  if (bracketNode->type != NODE_TYPE_BRACKET) {
    compilerError(cprocess, "Not a valid node in array of brackets \n");
    freeLexProcess(lexPr);
    freeCompileProcess(cprocess);
  }
  vector_push(brackets->nBrackets, &bracketNode);
}
struct vector *getArrayBracketsNodeVec(struct arrayBrackets *brackets) {
  return brackets->nBrackets;
}
size_t arrayBracketsCalculateSizeFromIndex(struct datatype *dtype,
                                           struct arrayBrackets *brackets,
                                           int index) {
  struct vector *arrayVec = getArrayBracketsNodeVec(brackets);
  size_t size = dtype->size;
  if (index >= vector_count(arrayVec)) {
    return size;
  }
  vector_set_peek_pointer(arrayVec, index);
  node *arrayBracketNode = vector_peek_ptr(arrayVec);
  if (!arrayBracketNode) {
    return 0;
  }
  while (arrayBracketNode) {

    if (arrayBracketNode->bracket.inner->type != NODE_TYPE_NUMBER) {
      compilerError(cprocess, "Not a number inside array brackets\n");
      freeLexProcess(lexPr);
      freeCompileProcess(cprocess);
      exit(-1);
    }

    int number = arrayBracketNode->bracket.inner->llnum;
    size *= number;
    arrayBracketNode = vector_peek_ptr(arrayVec);
  }
  return size;
}

size_t arrayBracketsCalcSize(struct datatype *dtype,
                             struct arrayBrackets *brackets) {
  return arrayBracketsCalculateSizeFromIndex(dtype, brackets, 0);
}
int arrayTotalIndexes(struct datatype *dtype) {
  if (!(dtype->flags & NODE_TYPE_BRACKET)) {
    compilerError(cprocess, "The datatype is not a bracket \n");
    freeLexProcess(lexPr);
    freeCompileProcess(cprocess);
    exit(-1);
  }
  struct arrayBrackets *brackets = dtype->array.brackets;
  return vector_count(brackets->nBrackets);
}
