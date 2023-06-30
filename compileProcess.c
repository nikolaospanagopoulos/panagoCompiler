#include "compileProcess.h"
#include "compiler.h"
#include "node.h"
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

void freeCompileProcess(compileProcess *cp) {
  if (cp->cfile.fp) {
    fclose(cp->cfile.fp);
  }
  if (cp->outFile) {
    fclose(cp->outFile);
  }

  vector_free(cp->nodeTreeVec);
  vector_free(cp->nodeVec);
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

  return process;
}
