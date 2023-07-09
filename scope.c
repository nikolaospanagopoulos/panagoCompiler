#include "compileProcess.h"
#include "compiler.h"
#include "vector.h"
#include <memory.h>
#include <stddef.h>
#include <stdlib.h>

scope *scopeAlloc() {
  scope *scope = calloc(1, sizeof(struct scope));
  scope->entities = vector_create(sizeof(void *));
  vector_set_peek_pointer_end(scope->entities);
  vector_set_flag(scope->entities, VECTOR_FLAG_PEEK_DECREMENT);
  return scope;
}

scope *createRootScope(compileProcess *cp) {
  if (cp->scope.root || cp->scope.current) {
    return NULL;
  }
  scope *rootScope = scopeAlloc();
  cp->scope.root = rootScope;
  cp->scope.current = rootScope;
  return rootScope;
}
void scopeDealloc(scope *scope) {
  if (scope) {
    vector_free(scope->entities);
    free(scope);
  }
}
void scopeFreeRoot(compileProcess *cp) {
  scopeDealloc(cp->scope.root);
  cp->scope.root = NULL;
  cp->scope.current = NULL;
}
scope *scopeNew(compileProcess *process, int flags) {
  if (!process->scope.root || !process->scope.current) {
    return NULL;
  }
  scope *newScope = scopeAlloc();
  newScope->flags = flags;
  newScope->parent = process->scope.current;
  process->scope.current = newScope;
  return newScope;
}
void scopeIterationStarts(scope *scope) {
  vector_set_peek_pointer(scope->entities, 0);
  if (scope->entities->flags & VECTOR_FLAG_PEEK_DECREMENT) {
    vector_set_peek_pointer_end(scope->entities);
  }
}
void *scopeIteratorBack(scope *scope) {
  if (vector_count(scope->entities) == 0) {
    return NULL;
  }
  return vector_peek_ptr(scope->entities);
}
void *scopeLastEntityAtScope(scope *scope) {
  if (vector_count(scope->entities) == 0) {
    return NULL;
  }
  return vector_back_ptr(scope->entities);
}
void *scopeLastEntityFromScopeStopAt(struct scope *scope,
                                     struct scope *stopScope) {
  if (scope == stopScope) {
    return NULL;
  }
  void *last = scopeLastEntityAtScope(scope);
  if (last) {
    return last;
  }
  struct scope *parent = scope->parent;
  if (parent) {
    return scopeLastEntityFromScopeStopAt(parent, stopScope);
  }
  return NULL;
}
void *scopeLastEntityStopAt(compileProcess *cp, scope *stopScope) {
  return scopeLastEntityFromScopeStopAt(cp->scope.current, stopScope);
}
void *scopeLastEntity(compileProcess *cp) {
  return scopeLastEntityStopAt(cp, NULL);
}
void scopePush(compileProcess *cp, void *ptr, size_t elemSize) {
  vector_push(cp->scope.current->entities, &ptr);
  cp->scope.current->size += elemSize;
}

void parserScopeFinish(compileProcess *currentProcess) {
  scopeFinish(currentProcess);
}
void scopeFinish(compileProcess *cp) {
  scope *newCurrentScope = cp->scope.current->parent;
  scopeDealloc(cp->scope.current);
  cp->scope.current = newCurrentScope;
  // Maybe free vector;
  if (cp->scope.root && !cp->scope.current) {
    cp->scope.root = NULL;
  }
}
scope *getCurrent(compileProcess *cp) { return cp->scope.current; }
