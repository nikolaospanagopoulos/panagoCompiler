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
  if (node == NULL) {
    return; // Base case: reached the end of the tree
  }

  freeNode(node->exp.left);  // Recursively free the left subtree
  freeNode(node->exp.right); // Recursively free the right subtree

  // Free any other dynamically allocated members of the node here
  // ...

  free(node); // Free the current node
}
void printNode(struct node *node) {
  if (node == NULL) {
    return; // Base case: reached the end of the tree
  }

  printNode(node->exp.left);

  if (node->type == NODE_TYPE_STRING || node->type == NODE_TYPE_IDENTIFIER) {
    printf("%s \n", node->sval);
  }
  if (node->type == NODE_TYPE_NUMBER) {
    printf("%llu \n", node->llnum);
  }

  printNode(node->exp.right);
}
void printNodeTreeVector(compileProcess *cp) {

  vector_set_peek_pointer(cp->nodeTreeVec, 0);
  struct node **node = (struct node **)vector_peek(cp->nodeTreeVec);
  while (node) {
    printNode(*node);

    node = (struct node **)vector_peek(cp->nodeTreeVec);
  }
}
void freeCompileProcess(compileProcess *cp) {
  if (cp->cfile.fp) {
    fclose(cp->cfile.fp);
  }
  if (cp->outFile) {
    fclose(cp->outFile);
  }
  printNodeTreeVector(cp);

  vector_set_peek_pointer(cp->nodeTreeVec, 0);
  struct node **node = (struct node **)vector_peek(cp->nodeTreeVec);
  while (node) {
    freeNode((*node));
    node = (struct node **)vector_peek(cp->nodeTreeVec);
  }
  vector_free(cp->nodeVec);
  vector_free(cp->nodeTreeVec);
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
