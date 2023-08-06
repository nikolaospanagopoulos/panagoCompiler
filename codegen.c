#include "compileProcess.h"
#include "compiler.h"
#include "node.h"
#include "vector.h"
#include <stdarg.h>
#include <stdio.h>

static struct compileProcess *currentProcess = NULL;

void codegenNewScope(int flags) {}
void codegenFinishScope() {}

void asmPushArgs(const char *ins, va_list args) {
  va_list args2;
  va_copy(args2, args);
  vfprintf(stdout, ins, args);
  fprintf(stdout, "\n");
  if (currentProcess->outFile) {
    vfprintf(currentProcess->outFile, ins, args2);
    fprintf(currentProcess->outFile, "\n");
  }
}

void asmPush(const char *ins, ...) {
  va_list args;
  va_start(args, ins);
  asmPushArgs(ins, args);
  va_end(args);
}
struct node *codegenNextNode() {
  return vector_peek_ptr(currentProcess->nodeTreeVec);
}
void generateDataSectionPart(struct node *node) {}
void generateDataSection() {
  asmPush("section .data");
  struct node *node = codegenNextNode();
  while (node) {
    generateDataSectionPart(node);
    node = codegenNextNode();
  }
}
void codegenWriteStrings() {}
void generateRod() {
  asmPush("section .rodata");
  codegenWriteStrings();
}
void codegenGenerateRootNode(struct node *node) {}
void codegenGenerateRoot() {
  asmPush("section .text");
  struct node *node = NULL;
  while ((node = codegenNextNode()) != NULL) {
    codegenGenerateRootNode(node);
  }
}
int codegen(struct compileProcess *process) {
  currentProcess = process;
  createRootScope(process);
  vector_set_peek_pointer(process->nodeTreeVec, 0);
  codegenNewScope(0);
  generateDataSection();
  vector_set_peek_pointer(process->nodeTreeVec, 0);
  codegenGenerateRoot();
  codegenFinishScope();
  generateRod();
  return CODEGEN_ALL_OK;
}
