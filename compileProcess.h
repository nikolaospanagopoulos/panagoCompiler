#pragma once
#include "stdio.h"
#include "token.h"

enum { COMPILER_FILE_COMPILED_OK, COMPILER_FILE_COMPILED_WITH_ERRORS };
typedef struct scope {
  int flags;
  struct vector *entities;
  size_t size;
  struct scope *parent;

} scope;

enum { SYMBOL_TYPE_NODE, SYMBOL_TYPE_NATIVE_FUNCTION, SYMBOL_TYPE_UNKNOWN };

typedef struct symbol {
  const char *name;
  int type;
  void *data;
} symbol;

typedef struct compileProcess {
  int flags;
  pos position;
  struct compileProcessInputFile {
    FILE *fp;
    const char *absolutePath;
  } cfile;
  FILE *outFile;
  struct fixupSystem *parserFixupSystem;
  struct vector *tokenVec;
  struct vector *nodeVec;
  struct vector *nodeTreeVec;
  struct vector *nodeGarbageVec;
  struct vector *gb;
  struct vector *gbForVectors;
  struct {
    scope *root;
    scope *current;
  } scope;
  struct {
    struct vector *table;
    struct vector *tables;
  } symbols;
} compileProcess;

void freeCompileProcess(compileProcess *cp);
compileProcess *compileProcessCreate(const char *filename,
                                     const char *filenameOutName, int flags);
