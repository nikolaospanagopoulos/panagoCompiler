#include "compileProcess.h"
#include "compiler.h"
#include "node.h"
#include "vector.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static struct compileProcess *cp;
void setCompileProcessForResolver(compileProcess *process) { cp = process; }

bool resolverResultFailed(struct resolverResult *result) {
  return result->flags & RESOLVER_RESULT_FLAG_FAILED;
}
bool resolverResultOK(struct resolverResult *result) {
  return !resolverResultFailed(result);
}

bool resolverResultFinished(struct resolverResult *result) {
  return result->flags & RESOLVER_RESULT_FLAG_RUNTIME_NEEDED_TO_FINISH_PATH;
}

struct resolverEntity *resolverResultEntityRoot(struct resolverResult *result) {
  return result->entity;
}

struct resolverEntity *resolverResultEntityNext(struct resolverEntity *entity) {
  return entity->next;
}

struct resolverEntity *resolverEntityClone(struct resolverEntity *entity) {
  if (!entity) {
    return NULL;
  }
  struct resolverEntity *newEntity = calloc(1, sizeof(struct resolverEntity));
  memcpy(newEntity, entity, sizeof(struct resolverEntity));
  // copy ptr to heap obj
  vector_push(cp->gb, &newEntity);
  return newEntity;
}

struct resolverEntity *resolverResultEntity(struct resolverResult *result) {
  if (resolverResultFailed(result)) {
    return NULL;
  }

  return result->entity;
}

struct resolverResult *resolverNewResult(struct resolverProcess *process) {
  struct resolverResult *result = calloc(1, sizeof(struct resolverResult));
  struct vector *arrayEntities = vector_create(sizeof(struct resolverEntity *));

  /*
  // garbage collection
  vector_push(cp->gbForVectors, arrayEntities);
  */

  result->arrayData.arrayEntities = arrayEntities;
  return result;
}

void resolverResultFree(struct resolverResult *result) {
  vector_free(result->arrayData.arrayEntities);
  free(result);
}
void resolverRuntimeNeeded(struct resolverResult *result,
                           struct resolverEntity *lastEntity) {
  result->entity = lastEntity;
  result->flags &= ~RESOLVER_RESULT_FLAG_RUNTIME_NEEDED_TO_FINISH_PATH;
}

void resolverResultEntityPush(struct resolverResult *result,
                              struct resolverEntity *entity) {
  if (!result->firstEntityConst) {
    result->firstEntityConst = entity;
  }
  if (!result->lastEntity) {
    result->entity = entity;
    result->lastEntity = entity;
    result->count++;
    return;
  }
  result->lastEntity->next = entity;
  entity->prev = result->lastEntity;
  result->lastEntity = entity;
  result->count++;
}
struct resolverEntity *resolverResultPeek(struct resolverResult *result) {
  return result->lastEntity;
}

struct resolverEntity *
resolverResultPeekIgnoreRuleEntity(struct resolverResult *result) {
  struct resolverEntity *entity = resolverResultPeek(result);
  while (entity && entity->type == RESOLVER_ENTITY_TYPE_RULE) {
    entity = entity->prev;
  }
  return entity;
}

struct resolverEntity *resolverResultPop(struct resolverResult *result) {
  // TODO: check memory here
  struct resolverEntity *entity = result->lastEntity;
  if (!result->entity) {
    return NULL;
  }
  if (result->entity == result->lastEntity) {
    result->entity = result->lastEntity->prev;
    result->lastEntity = result->lastEntity->prev;
    result->count--;
    goto out;
  }
  result->lastEntity = result->lastEntity->prev;
  result->count--;

out:
  if (result->count == 0) {
    result->firstEntityConst = NULL;
    result->lastEntity = NULL;
    result->entity = NULL;
  }
  entity->prev = NULL;
  entity->next = NULL;
  return entity;
}
struct vector *resolverArrayDataVec(struct resolverResult *result) {
  return result->arrayData.arrayEntities;
}

struct compileProcess *resolverCompiler(struct resolverProcess *process) {
  return process->cmpProcess;
}

struct resolverScope *resolverScopeCurrent(struct resolverProcess *process) {
  return process->scope.current;
}

struct resolverScope *resolverScopeRoot(struct resolverProcess *process) {
  return process->scope.root;
}

struct resolverScope *resolverNewScopeCreate() {
  // TODO: cleanup needed ???
  struct resolverScope *scope = calloc(1, sizeof(struct resolverScope));
  scope->entities = vector_create(sizeof(struct resolverEntity));
  return scope;
}
struct resolverScope *resolverNewScope(struct resolverProcess *resolver,
                                       void *private, int flags) {
  struct resolverScope *scope = resolverNewScopeCreate();
  if (!scope) {
    return NULL;
  }
  resolver->scope.current->next = scope;
  scope->prev = resolver->scope.current;
  resolver->scope.current = scope;
  scope->privateData = private;
  scope->flags = flags;
  return scope;
}
void resolverFinishScope(struct resolverProcess *resolver) {
  struct resolverScope *scope = resolver->scope.current;
  resolver->scope.current = scope->prev;
  resolver->callbacks.delete_scope(scope);
  free(scope);
}

struct resolverProcess *
resolverNewProcess(struct compileProcess *compiler,
                   struct resolverCallbacks *callbacks) {
  struct resolverProcess *process = calloc(1, sizeof(struct resolverProcess));
  process->cmpProcess = compiler;
  memcpy(&process->callbacks, callbacks, sizeof(process->callbacks));
  process->scope.root = resolverNewScopeCreate();
  process->scope.current = process->scope.root;
  return process;
}

struct resolverEntity *resolverCreateNewEntity(struct resolverResult *result,
                                               int type, void *private) {
  struct resolverEntity *entity = calloc(1, sizeof(struct resolverEntity));
  if (!entity) {
    return NULL;
  }
  entity->type = type;
  entity->privateData = private;
  return entity;
}
struct resolverEntity *
resolverCreateNewEntityForUnsupportedNodes(struct resolverResult *result,
                                           struct node *node) {
  struct resolverEntity *entity =
      // TODO: maybe cleanup
      resolverCreateNewEntity(result, RESOLVER_ENTITY_TYPE_UNSUPPORTED, NULL);
  if (!entity) {
    return NULL;
  }
  entity->node = node;
  entity->flags = RESOLVER_ENTITY_FLAG_NO_MERGE_WITH_LEFT_ENTITY |
                  RESOLVER_ENTITY_FLAG_NO_MERGE_WITH_NEXT_ENTITY;
  return entity;
}
struct resolverEntity *resolverCreateNewEntityForArrayBracket(
    struct resolverResult *result, struct resolverProcess *process,
    struct node *node, struct node *arrayIndexNode, int index,
    struct datatype *dtype, void *privateData, struct resolverScope *scope) {

  struct resolverEntity *entity = resolverCreateNewEntity(
      result, RESOLVER_ENTITY_TYPE_ARRAY_BRACKET, privateData);

  if (!entity) {
    return NULL;
  }

  entity->scope = scope;
  if (!entity->scope) {
    compilerError(cp, "Resolver entity scope error\n");
  }
  entity->name = NULL;
  entity->dtype = *dtype;
  entity->node = node;
  entity->array.index = index;
  entity->array.dtype = *dtype;
  entity->array.arrayIndexNode = arrayIndexNode;
  int arrayIndexVal = 1;
  if (arrayIndexNode->type == NODE_TYPE_NUMBER) {
    arrayIndexVal = arrayIndexNode->llnum;
  }
  // entity->offset = arrayOffset(dtype, index, arrayIndexVal);
  return entity;
}
struct resolverEntity *resolverCreateNewEntityForMergedArrayBracket(
    struct resolverResult *result, struct resolverProcess *process,
    struct node *node, struct node *arrayIndexNode, int index,
    struct datatype *dtype, void *privateData, struct resolverScope *scope) {

  struct resolverEntity *entity = resolverCreateNewEntity(
      result, RESOLVER_ENTITY_TYPE_ARRAY_BRACKET, privateData);
  if (!entity) {
    return NULL;
  }
  entity->scope = scope;
  if (!entity->scope) {
    compilerError(cp, "No entity scope for the resolver \n");
  }
  entity->name = NULL;
  entity->dtype = *dtype;
  entity->node = node;
  entity->array.index = index;
  entity->array.dtype = *dtype;
  entity->array.arrayIndexNode = arrayIndexNode;
  return entity;
}

struct resolverEntity *
resolverCreateNewUnknownEntity(struct resolverProcess *process,
                               struct resolverResult *result,
                               struct datatype *dtype, struct node *node,
                               struct resolverScope *scope, int offset) {
  struct resolverEntity *entity =
      resolverCreateNewEntity(result, RESOLVER_ENTITY_TYPE_GENERAL, NULL);
  if (!entity) {
    return NULL;
  }
  entity->flags |= RESOLVER_ENTITY_FLAG_NO_MERGE_WITH_NEXT_ENTITY |
                   RESOLVER_ENTITY_FLAG_NO_MERGE_WITH_LEFT_ENTITY;

  entity->scope = scope;
  entity->dtype = *dtype;
  entity->node = node;
  entity->offset = offset;
  return entity;
}
struct resolverEntity *resolverCreateNewUnaryIndirectionEntity(
    struct resolverProcess *process, struct resolverResult *result,
    struct node *node, int indirectionDepth) {

  struct resolverEntity *entity = resolverCreateNewEntity(
      result, RESOLVER_ENTITY_TYPE_UNARY_INDIRECTION, NULL);
  if (!entity) {
    return NULL;
  }
  entity->flags = RESOLVER_ENTITY_FLAG_NO_MERGE_WITH_LEFT_ENTITY |
                  RESOLVER_ENTITY_FLAG_NO_MERGE_WITH_NEXT_ENTITY;

  entity->node = node;
  entity->indirection.depth = indirectionDepth;
  return entity;
}
struct resolverEntity *resolverCreateNewUnaryGetAddressEntity(
    struct resolverProcess *process, struct resolverResult *result,
    struct datatype *dtype, struct node *node, struct resolverScope *scope,
    int offset) {

  struct resolverEntity *entity = resolverCreateNewEntity(
      result, RESOLVER_ENTITY_TYPE_UNARY_GET_ADDRESS, NULL);
  if (!entity) {
    return NULL;
  }
  entity->flags = RESOLVER_ENTITY_FLAG_NO_MERGE_WITH_LEFT_ENTITY |
                  RESOLVER_ENTITY_FLAG_NO_MERGE_WITH_NEXT_ENTITY;
  entity->node = node;
  entity->scope = scope;
  entity->dtype = *dtype;
  entity->dtype.flags |= DATATYPE_FLAG_IS_POINTER;
  entity->dtype.ptrDepth++;
  return entity;
}
struct resolverEntity *
resolverCreateNewCastEntity(struct resolverProcess *process,
                            struct resolverScope *scope,
                            struct datatype *castDtype) {
  struct resolverEntity *entity =
      resolverCreateNewEntity(NULL, RESOLVER_ENTITY_TYPE_CAST, NULL);

  if (!entity) {
    return NULL;
  }

  entity->flags = RESOLVER_ENTITY_FLAG_NO_MERGE_WITH_LEFT_ENTITY |
                  RESOLVER_ENTITY_FLAG_NO_MERGE_WITH_NEXT_ENTITY;
  entity->scope = scope;
  entity->dtype = *castDtype;
  return entity;
}

struct resolverEntity *resolverCreateNewEntityForVarNodeCustomScope(
    struct resolverProcess *process, struct node *varNode, void *privateData,
    struct resolverScope *scope, int offset) {
  if (varNode->type != NODE_TYPE_VARIABLE) {
    compilerError(cp, "Not a variable \n");
  }
  struct resolverEntity *entity =
      resolverCreateNewEntity(NULL, NODE_TYPE_VARIABLE, privateData);
  if (!entity) {
    return NULL;
  }
  entity->scope = scope;
  if (!entity->scope) {
    compilerError(cp, "No entity scope");
  }
  entity->dtype = varNode->var.type;
  entity->node = varNode;
  entity->name = varNode->var.name;
  entity->offset = offset;
  return entity;
}
struct resolverEntity *
resolverCreateNewEntityForVarNode(struct resolverProcess *process,
                                  struct node *varNode, void *privateData,
                                  int offset) {
  return resolverCreateNewEntityForVarNodeCustomScope(
      process, varNode, privateData, resolverScopeCurrent(process), offset);
}
struct resolverEntity *resolverCreateNewEntityForVarNodeNoPush(
    struct resolverProcess *process, struct node *varNode, void *privateData,
    int offset, struct resolverScope *scope) {
  struct resolverEntity *entity = resolverCreateNewEntityForVarNodeCustomScope(
      process, varNode, privateData, scope, offset);
  if (!entity) {
    return NULL;
  }
  if (scope->flags & RESOLVER_SCOPE_FLAG_IS_STACK) {
    entity->flags |= RESOLVER_SCOPE_FLAG_IS_STACK;
  }
  return entity;
}
struct resolverEntity *
resolverNewEntityForVarNode(struct resolverProcess *process,
                            struct node *varNode, void *privateData,
                            int offset) {
  struct resolverEntity *entity = resolverCreateNewEntityForVarNodeNoPush(
      process, varNode, privateData, offset, resolverScopeCurrent(process));
  if (!entity) {
    return NULL;
  }
  vector_push(process->scope.current->entities, &entity);
  return entity;
}
