#include "compileProcess.h"
#include "compiler.h"
#include "position.h"
#include "vector.h"
static struct compileProcess *currentProcess;

int parseNext() { return 1; }

int parse(compileProcess *process) {
  currentProcess = process;
  struct node *node = NULL;
  vector_set_peek_pointer(process->tokenVec, 0);
  while (parseNext() == 0) {
    // node = nodePeek();
    vector_push(process->nodeTreeVec, &node);
  }
  return PARSE_ALL_OK;
}
