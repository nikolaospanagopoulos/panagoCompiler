#include "compileProcess.h"
#include "compiler.h"
#include "node.h"
#include "string.h"
#include "vector.h"
#include <stdlib.h>

static void symResolverPushSymbol(compileProcess *process, struct symbol *sym) {
  // CLEANUP
  vector_push(process->symbols.table, &sym);
}

void symResolverInitialize(struct compileProcess *process) {
  process->symbols.tables = vector_create(sizeof(struct vector *));
  vector_push(process->gbForVectors, &process->symbols.tables);
}

void symresolverNewTable(compileProcess *process) {
  vector_push(process->symbols.tables, &process->symbols.table);
  process->symbols.table = vector_create(sizeof(struct symbol *));
  vector_push(process->gbForVectors, &process->symbols.table);
}
void symresolverEndTable(compileProcess *process) {
  struct vector *lastTable = vector_back_ptr(process->symbols.tables);
  process->symbols.table = lastTable;
  vector_pop(process->symbols.tables);
}

symbol *symresolverGetSymbol(compileProcess *process, const char *name) {
  vector_set_peek_pointer(process->symbols.table, 0);
  symbol *sym = vector_peek_ptr(process->symbols.table);
  while (sym) {
    if (S_EQ(sym->name, name)) {
      break;
    }
    sym = vector_peek_ptr(process->symbols.table);
  }

  return sym;
}
symbol *symresolverGetSymbolForNativeFunction(compileProcess *process,
                                              const char *name) {
  symbol *sym = symresolverGetSymbol(process, name);
  if (!sym) {
    return NULL;
  }
  if (sym->type != SYMBOL_TYPE_NATIVE_FUNCTION) {
    return NULL;
  }
  return sym;
}
symbol *symresolverRegisterSymbol(compileProcess *process, const char *symName,
                                  int type, void *data) {
  if (symresolverGetSymbol(process, symName)) {
    return NULL;
  }
  symbol *sym = calloc(1, sizeof(struct symbol));
  vector_push(process->gb, &sym);
  sym->name = symName;
  sym->type = type;
  sym->data = data;
  symResolverPushSymbol(process, sym);
  return sym;
}
node *symresolverNode(symbol *sym) {
  if (sym->type != SYMBOL_TYPE_NODE) {
    return NULL;
  }
  return sym->data;
}
void symresolverBuildForVariableNode(compileProcess *process, node *node) {}
void symresolverBuildForFunctionNode(compileProcess *process, node *node) {}
void symresolverBuildForStructureNode(compileProcess *process, node *node) {
  if (node->flags & NODE_FLAG_IS_FORWARD_DECLERATION) {
    return;
  }
  symresolverRegisterSymbol(process, node->_struct.name, SYMBOL_TYPE_NODE,
                            node);
}
void symresolverBuildForUnionNode(compileProcess *process, node *node) {

  if (node->flags & NODE_FLAG_IS_FORWARD_DECLERATION) {
    return;
  }
  symresolverRegisterSymbol(process, node->_union.name, SYMBOL_TYPE_NODE, node);
}
void symresolverBuildForNode(compileProcess *process, node *node) {
  switch (node->type) {
  case NODE_TYPE_VARIABLE:
    symresolverBuildForVariableNode(process, node);
    break;
  case NODE_TYPE_FUNCTION:
    symresolverBuildForFunctionNode(process, node);
    break;
  case NODE_TYPE_STRUCT:
    symresolverBuildForStructureNode(process, node);
    break;
  case NODE_TYPE_UNION:
    symresolverBuildForUnionNode(process, node);
    break;
  }
}
