#include "compiler.h"
#include "node.h"
#include "vector.h"
#include <stddef.h>
#include <string.h>

#define STACK_PUSH_SIZE 4

static struct compileProcess *currentProcess = NULL;

void setCompileProcessForStackFrame(struct compileProcess *cp) {
  currentProcess = cp;
}

void stackFramePop(struct node *funcNode) {
  struct stackFrame *frame = &funcNode->func.frame;
  vector_pop(frame->elements);
}
struct stackFrameElement *stackframeBack(struct node *funcNode) {
  return vector_back_or_null(funcNode->func.frame.elements);
}
void stackFramePeekStart(struct node *funcNode) {
  struct stackFrame *frame = &funcNode->func.frame;
  vector_set_peek_pointer_end(frame->elements);
  vector_set_flag(frame->elements, VECTOR_FLAG_PEEK_DECREMENT);
}
struct stackFrameElement *stackframePeek(struct node *funcNode) {
  struct stackFrame *frame = &funcNode->func.frame;
  return vector_peek(frame->elements);
}
struct stackFrameElement *stackFrameBackExpect(struct node *funcNode,
                                               int expectingType,
                                               const char *expectingName) {
  struct stackFrameElement *element = stackframeBack(funcNode);
  if ((element && element->type != expectingType) ||
      (!S_EQ(element->name, expectingName))) {
    return NULL;
  }
  return element;
}
void stackFramePopExpecting(struct node *funcNode, int expectingType,
                            const char *expectingName) {
  struct stackFrame *frame = &funcNode->func.frame;
  struct stackFrameElement *lastElement = stackframeBack(funcNode);
  if (!lastElement) {
    compilerError(currentProcess,
                  "Stackframe error: last element doesnt exist \n");
  }
  if (lastElement->type != expectingType &&
      !S_EQ(lastElement->name, expectingName)) {
    compilerError(currentProcess,
                  "Stackframe error: expecting wrong element\n");
  }
  stackFramePop(funcNode);
}

void stackframePush(struct node *funcNode, struct stackFrameElement *element) {
  struct stackFrame *frame = &funcNode->func.frame;
  element->offsetFromBasePtr =
      -(vector_count(frame->elements) * STACK_PUSH_SIZE);
  vector_push(frame->elements, element);
}

void stackframeSub(struct node *funcNode, int type, const char *name,
                   size_t amount) {
  if ((amount % STACK_PUSH_SIZE) != 0) {
    compilerError(currentProcess, "Allignment issue\n");
  }
  size_t totalPushes = amount / STACK_PUSH_SIZE;
  for (size_t i = 0; i < totalPushes; i++) {
    stackframePush(funcNode,
                   &(struct stackFrameElement){.type = type, .name = name});
  }
}
void stackframeAdd(struct node *funcNode, int type, const char *name,
                   size_t amount) {
  if ((amount % STACK_PUSH_SIZE) != 0) {
    compilerError(currentProcess, "Allignment issue\n");
  }
  size_t totalPushes = amount / STACK_PUSH_SIZE;
  for (size_t i = 0; i < totalPushes; i++) {
    stackFramePop(funcNode);
  }
}
void stackFrameAssertEmpty(struct node *funcNode) {
  struct stackFrame *frame = &funcNode->func.frame;
  if (vector_count(frame->elements) != 0) {
    compilerError(currentProcess, "Stack is not empty error \n");
  }
}
