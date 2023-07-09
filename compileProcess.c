#include "compileProcess.h"
#include "compiler.h"
#include "node.h"
#include "token.h"
#include "vector.h"
#include <stdio.h>
#include <stdlib.h>

char compileProcessNextChar(lexProcess *process) {
  compileProcess *cProcess = process->compiler;
  cProcess->position.col += 1;
  char c = getc(cProcess->cfile.fp);
  if (c == '\n') {
    cProcess->position.line += 1;
    cProcess->position.col = 1;
  }
  return c;
}

char compileProcessPeekChar(lexProcess *process) {
  compileProcess *cProcess = process->compiler;
  char c = getc(cProcess->cfile.fp);
  ungetc(c, cProcess->cfile.fp);
  return c;
}

void compileProcessPushChar(lexProcess *process, char c) {
  compileProcess *cProcess = process->compiler;
  ungetc(c, cProcess->cfile.fp);
}
void freeNode(struct node *node) {
  if (!node) {
    return;
  }

  if (node->type == NODE_TYPE_VARIABLE_LIST) {
    vector_free(node->varlist.list);
  }

  if (node->body.statements != NULL) {
    vector_free(node->body.statements);
  }
  if (node->var.type.array.brackets &&
      node->var.type.array.brackets->nBrackets) {
    vector_free(node->var.type.array.brackets->nBrackets);
    free(node->var.type.array.brackets);
  }
  if (node) {
    free(node);
  }
}
void freeCompileProcess(compileProcess *cp) {
  if (cp->cfile.fp) {
    fclose(cp->cfile.fp);
  }
  if (cp->outFile) {
    fclose(cp->outFile);
  }

  vector_set_peek_pointer(cp->nodeGarbageVec, 0);
  node **node = (struct node **)vector_peek(cp->nodeGarbageVec);
  while (node) {

    freeNode(*node);
    node = (struct node **)vector_peek(cp->nodeGarbageVec);
  }
  vector_set_peek_pointer(cp->gb, 0);
  void **data = (void **)vector_peek(cp->gb);
  while (data) {
    free(*data);
    data = (void **)vector_peek(cp->gb);
  }
  vector_free(cp->nodeTreeVec);
  vector_free(cp->nodeVec);
  vector_free(cp->nodeGarbageVec);
  scopeFreeRoot(cp);
  vector_free(cp->gb);
}

compileProcess *compileProcessCreate(const char *filename,
                                     const char *filenameOutName, int flags) {
  FILE *file = fopen(filename, "r");
  if (!file) {
    return NULL;
  }
  FILE *outFile = NULL;
  if (filenameOutName) {
    outFile = fopen(filenameOutName, "w");
    if (!outFile) {
      fclose(file);
      return NULL;
    }
  }
  compileProcess *process = calloc(1, sizeof(compileProcess));
  process->flags = flags;
  process->cfile.fp = file;
  process->outFile = outFile;
  process->nodeVec = vector_create(sizeof(struct node *));
  process->nodeTreeVec = vector_create(sizeof(struct node *));
  process->nodeGarbageVec = vector_create(sizeof(struct node *));
  process->gb = vector_create(sizeof(void *));
  return process;
}
