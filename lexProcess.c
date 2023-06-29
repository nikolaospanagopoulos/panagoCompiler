#include "buffer.h"
#include "compileProcess.h"
#include "compiler.h"
#include "token.h"
#include "vector.h"
#include <stdio.h>
#include <stdlib.h>

lexProcess *lexProcessCreate(compileProcess *compiler,
                             struct lexProcessFunctions *functions,
                             void *privateData) {

  lexProcess *process = calloc(1, sizeof(lexProcess));
  process->function = functions;
  process->tokenVec = vector_create(sizeof(struct token));
  process->compiler = compiler;
  process->privateData = privateData;
  process->pos.line = 1;
  process->pos.col = 1;
  process->parenthesesBuffer = NULL;
  return process;
}
void freeLexProcess(lexProcess *process) {
  vector_set_peek_pointer(process->tokenVec, 0);
  struct token *tok = vector_peek(process->tokenVec);

  while (tok) {
    if (tok->type == STRING || tok->type == OPERATOR ||
        tok->type == IDENTIFIER || tok->type == KEYWORD ||
        tok->type == COMMENT) {
      free((char *)tok->sval);
    }
    tok = vector_peek(process->tokenVec);
  }
  vector_free(process->tokenVec);
  free(process);
}

void *lexProcessPrivateData(lexProcess *process) {
  return process->privateData;
}

struct vector *lexProcessTokens(lexProcess *process) {
  return process->tokenVec;
}
