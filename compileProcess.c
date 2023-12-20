#include "compileProcess.h"
#include "compiler.h"
#include "fixup.h"
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

  if (node->var.type.array.brackets &&
      node->var.type.array.brackets->nBrackets) {
    vector_free(node->var.type.array.brackets->nBrackets);
    free(node->var.type.array.brackets);
  }
  if (node) {
    free(node);
  }
}

void freeVectorContentsVectors(struct vector *vecToFree) {
  struct vector **data = (struct vector **)vector_peek(vecToFree);
  while (data) {
    vector_free(*data);
    data = (struct vector **)vector_peek(vecToFree);
  }
}
void freeVectorContents(struct vector *vecToFree) {
  vector_set_peek_pointer(vecToFree, 0);
  void **data = (void **)vector_peek(vecToFree);
  while (data) {
    free(*data);
    data = (void **)vector_peek(vecToFree);
  }
}
void freeResolverEntitiesVector(struct vector *vecToFree) {
  vector_set_peek_pointer(vecToFree, 0);
  struct resolverEntity **data =
      (struct resolverEntity **)vector_peek(vecToFree);
  while (data) {
    if ((*data)->privateData) {
      free((*data)->privateData);
    }
    free(*data);
    data = (struct resolverEntity **)vector_peek(vecToFree);
  }
}

/*
void freeVectorStringTable(struct vector *strTable) {
  struct stringTableElement **data =
      (struct stringTableElement **)vector_peek(strTable);
  while (data) {
    free((char *)(*data)->str);
    free((char *)(*data)->label);
    free(*data);
    data = (struct stringTableElement **)vector_peek(strTable);
  }
}
*/
void codegenFree(compileProcess *process) {
  vector_free(process->generator->exitPoints);
  vector_free(process->generator->entryPoints);
  // freeVectorStringTable(process->generator->stringTable);
  freeVectorContents(process->generator->stringTable);
  vector_free(process->generator->stringTable);
  free(process->generator);
}

void freeResolverTrackedScopes(compileProcess *process) {
  vector_set_peek_pointer(process->trackedScopes, 0);
  struct resolverScope **data =
      (struct resolverScope **)vector_peek(process->trackedScopes);
  while (data) {
    if ((*data)->entities && vector_count((*data)->entities) > 0) {

      freeResolverEntitiesVector((*data)->entities);
      vector_free((*data)->entities);
    }
    if (*data) {
      free(*data);
    }
    data = (struct resolverScope **)vector_peek(process->trackedScopes);
  }
}

void resolverFree(compileProcess *process) {

  freeResolverTrackedScopes(process);
  vector_free(process->trackedScopes);

  free(process->resolver);
}
void freeCompileProcess(compileProcess *cp) {
  if (cp->cfile.fp) {
    fclose(cp->cfile.fp);
  }
  if (cp->outFile) {
    fclose(cp->outFile);
  }

  freeVectorContents(cp->nodeGarbageVec);
  freeVectorContents(cp->gb);
  vector_free(cp->nodeTreeVec);
  vector_free(cp->nodeVec);
  vector_free(cp->nodeGarbageVec);
  vector_set_peek_pointer(cp->gbForVectors, 0);
  freeVectorContentsVectors(cp->gbForVectors);
  vector_free(cp->gbForVectors);

  scopeFreeRoot(cp);
  fixupSystemFree(cp->parserFixupSystem);

  vector_free(cp->gb);
  resolverFree(cp);
  codegenFree(cp);
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
  process->gbForVectors = vector_create(sizeof(struct vector *));
  process->generator = codegenNew(process);
  process->resolver = resolverDefaultNewProcess(process);
  symResolverInitialize(process);
  symresolverNewTable(process);

  return process;
}
