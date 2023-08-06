#include "compileProcess.h"
#include "compiler.h"
#include "node.h"
#include "vector.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

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
static const char *asmKeywordForSize(size_t size, char *tmpBuff) {
  const char *keyword = NULL;
  switch (size) {
  case DATA_SIZE_BYTE:
    keyword = "db";
    break;
  case DATA_SIZE_WORD:
    keyword = "dw";
    break;
  case DATA_SIZE_DWORD:
    keyword = "dd";
    break;
  case DATA_SIZE_DDWORD:
    keyword = "dq";
    break;
  default:
    sprintf(tmpBuff, "times %ld db ", (unsigned long)size);
    return tmpBuff;
  }
  strcpy(tmpBuff, keyword);
  return tmpBuff;
}
struct node *codegenNextNode() {
  return vector_peek_ptr(currentProcess->nodeTreeVec);
}
void codegenGlobalVariablePrimitive(struct node *node) {
  char tmpBuff[256];
  if (node->var.val != NULL) {
    if (node->var.val->type == NODE_TYPE_STRING) {

    } else {
      // its a numeric value
    }
  }
  asmPush("%s: %s 0", node->var.name,
          asmKeywordForSize(variableSize(node), tmpBuff));
}
void codegenGenerateGlobalVariable(struct node *node) {
  asmPush("; type: %s, name: %s\n", node->var.type.typeStr, node->var.name);
  switch (node->var.type.type) {
  case DATA_TYPE_VOID:
  case DATA_TYPE_CHAR:
  case DATA_TYPE_SHORT:
  case DATA_TYPE_INTEGER:
  case DATA_TYPE_LONG:
    codegenGlobalVariablePrimitive(node);
    break;
  case DATA_TYPE_DOUBLE:
  case DATA_TYPE_FLOAT:
    compilerError(currentProcess,
                  "Double and Float types are not supported \n");
    break;
  }
}
void generateDataSectionPart(struct node *node) {
  switch (node->type) {
  case NODE_TYPE_VARIABLE:
    codegenGenerateGlobalVariable(node);
    break;
  default:
    break;
  }
}
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
