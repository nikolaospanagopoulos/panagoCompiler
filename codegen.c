#include "compileProcess.h"
#include "compiler.h"
#include <stdarg.h>
#include <stdio.h>

static struct compileProcess *currentProcess = NULL;

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

int codegen(struct compileProcess *process) {
  currentProcess = process;
  asmPush("jmp %s", "label name");
  return CODEGEN_ALL_OK;
}
