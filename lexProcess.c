#include "compileProcess.h"
#include "compiler.h"
#include "vector.h"
#include <stdlib.h>

lexProcess *lexProcessCreate(compileProcess *compiler,
                             struct lexProcessFunctions *functions,
                             void *privateData) {

  lexProcess *process = calloc(1, sizeof(lexProcess));
  process->function = functions;
  process->tokenVec = vector_create(sizeof(token));
  process->compiler = compiler;
  process->privateData = privateData;
  process->pos.line = 1;
  process->pos.col = 1;
  return process;
}
void freeLexProcess(lexProcess *process) {
  vector_free(process->tokenVec);
  free(process);
}

void *lexProcessPrivateData(lexProcess *process) {
  return process->privateData;
}

struct vector *lexProcessTokens(lexProcess *process) {
  return process->tokenVec;
}
