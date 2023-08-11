#include "compileProcess.h"
#include "compiler.h"
#include "node.h"
#include "vector.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
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
struct codeGenerator *codegenNew(struct compileProcess *process) {
  struct codeGenerator *generator = calloc(1, sizeof(struct codeGenerator));
  generator->entryPoints = vector_create(sizeof(struct codegenEntryPoint *));
  generator->exitPoints = vector_create(sizeof(struct codegenExitPoint *));
  return generator;
}
void codeGeneratorRegisterExitPoint(int exitPointId) {
  struct codeGenerator *gen = currentProcess->generator;
  struct codegenExitPoint *exitPoint =
      calloc(1, sizeof(struct codegenExitPoint));
  exitPoint->id = exitPointId;
  vector_push(gen->exitPoints, &exitPoint);
}

void codeGeneratorRegisterEntryPoint(int entryPointId) {
  struct codeGenerator *gen = currentProcess->generator;
  struct codegenEntryPoint *entryPoint =
      calloc(1, sizeof(struct codegenEntryPoint));
  entryPoint->id = entryPointId;
  vector_push(gen->entryPoints, &entryPoint);
}
int codegenLabelCount() {
  static int count = 0;
  count++;
  return count;
}

struct codegenExitPoint *codegenCurrentExitPoint() {
  struct codeGenerator *gen = currentProcess->generator;
  return vector_back_ptr_or_null(gen->exitPoints);
}
struct codegenEntryPoint *codegenCurrentEntryPoint() {
  struct codeGenerator *gen = currentProcess->generator;
  return vector_back_ptr_or_null(gen->entryPoints);
}
void codegenBeginExitPoint() {
  int exitPointId = codegenLabelCount();
  codeGeneratorRegisterExitPoint(exitPointId);
}

void codegenBeginEntryPoint() {
  int entryPointId = codegenLabelCount();
  codeGeneratorRegisterEntryPoint(entryPointId);
  asmPush(".entry_point_%i:", entryPointId);
}
void codegenEndEntryPoint() {
  struct codeGenerator *gen = currentProcess->generator;
  struct codegenEntryPoint *entryPoint = codegenCurrentEntryPoint();
  if (!entryPoint) {
    compilerError(currentProcess, "no entry point to end \n");
  }
  free(entryPoint);
  vector_pop(gen->entryPoints);
}
void codegenBeginEntryExitPoint() {
  codegenBeginEntryPoint();
  codegenBeginExitPoint();
}

void codegenEndExitPoint() {
  struct codeGenerator *gen = currentProcess->generator;
  struct codegenExitPoint *exitPoint = codegenCurrentExitPoint();
  if (!exitPoint) {
    compilerError(currentProcess, "exit point doesnt exist \n");
  }
  asmPush(".exit_point_%i:", exitPoint->id);
  free(exitPoint);
  vector_pop(gen->exitPoints);
}
void codegenEndEntryExitPoint() {
  codegenEndEntryPoint();
  codegenEndExitPoint();
}
void codegenGotoEntryPoint(struct node *node) {
  struct codeGenerator *gen = currentProcess->generator;
  struct codegenEntryPoint *entryPoint = codegenCurrentEntryPoint();
  asmPush("jmp .entry_point_%i", entryPoint->id);
}
void codegenGoToExitPoint(struct node *node) {
  struct codeGenerator *gen = currentProcess->generator;
  struct codegenExitPoint *exitPoint = codegenCurrentExitPoint();
  asmPush("jmp .exit_point_%i", exitPoint->id);
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
  codegenBeginEntryExitPoint();
  codegenGoToExitPoint(NULL);
  codegenGotoEntryPoint(NULL);
  codegenEndEntryExitPoint();
  return CODEGEN_ALL_OK;
}
