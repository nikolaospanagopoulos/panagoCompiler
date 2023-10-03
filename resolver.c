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
void resolverNewEntityForRule(struct resolverProcess *process,
                              struct resolverResult *result,
                              struct resolverEntityRule *rule) {
  struct resolverEntity *entityRule =
      resolverCreateNewEntity(result, RESOLVER_ENTITY_TYPE_RULE, NULL);
  entityRule->rule = *rule;
  resolverResultEntityPush(result, entityRule);
}
struct resolverEntity *resolverMakeEntity(struct resolverProcess *process,
                                          struct resolverResult *result,
                                          struct datatype *customDtype,
                                          struct node *node,
                                          struct resolverEntity *guidedEntity,
                                          struct resolverScope *scope) {
  struct resolverEntity *entity = NULL;
  int offset = guidedEntity->offset;
  int flags = guidedEntity->flags;
  switch (node->type) {
  case NODE_TYPE_VARIABLE:
    entity = resolverCreateNewEntityForVarNodeNoPush(process, node, NULL,
                                                     offset, scope);
    break;
  default:
    entity = resolverCreateNewUnknownEntity(process, result, customDtype, node,
                                            scope, offset);
  }
  if (entity) {
    entity->flags |= flags;
    if (customDtype) {
      entity->dtype = *customDtype;
    }
    entity->privateData =
        process->callbacks.make_private(entity, node, offset, scope);
  }
  return entity;
}
struct resolverEntity *resolverCreateNewEntityForFunctionCall(
    struct resolverResult *result, struct resolverProcess *process,
    struct resolverEntity *leftOperandEntity, void *privateData) {
  struct resolverEntity *entity = resolverCreateNewEntity(
      result, RESOLVER_ENTITY_TYPE_FUNCTION_CALL, privateData);
  if (!entity) {
    return NULL;
  }
  entity->dtype = leftOperandEntity->dtype;
  entity->funcCallData.arguments = vector_create(sizeof(struct node *));
  return entity;
}
struct resolverEntity *resolverRegisterFunction(struct resolverProcess *process,
                                                struct node *funcNode,
                                                void *privateData) {
  struct resolverEntity *entity =
      resolverCreateNewEntity(NULL, RESOLVER_ENTITY_TYPE_FUNCTION, privateData);

  if (!entity) {
    return NULL;
  }
  entity->name = funcNode->func.name;
  entity->node = funcNode;
  entity->dtype = funcNode->func.rtype;
  entity->scope = resolverScopeCurrent(process);
  vector_push(process->scope.root->entities, &entity);
  return entity;
}
struct resolverEntity *resolverGetEntityInScopeWithEntityType(
    struct resolverResult *result, struct resolverProcess *rprocess,
    struct resolverScope *rscope, const char *entityName, int entityType) {
  if (result && result->lastStructUnionEntity) {
    struct resolverScope *scope = result->lastStructUnionEntity->scope;
    struct node *outNode = NULL;
    struct datatype *nodeVarDatatype = &result->lastStructUnionEntity->dtype;
    int offset =
        structOffset(resolverCompiler(rprocess), nodeVarDatatype->typeStr,
                     entityName, &outNode, 0, 0);
    if (nodeVarDatatype->type == DATA_TYPE_UNION) {
      offset = 0;
    }
    return resolverMakeEntity(
        rprocess, result, NULL, outNode,
        &(struct resolverEntity){.type = RESOLVER_ENTITY_TYPE_VARIABLE,
                                 .offset = offset},
        scope);
  }
  vector_set_peek_pointer_end(rscope->entities);
  vector_set_flag(rscope->entities, VECTOR_FLAG_PEEK_DECREMENT);
  struct resolverEntity *current = vector_peek_ptr(rscope->entities);
  while (current) {
    if (entityType != -1 && current->type != entityType) {
      current = vector_peek_ptr(rscope->entities);
      continue;
    }
    if (S_EQ(current->name, entityName)) {
      break;
    }
    current = vector_peek_ptr(rscope->entities);
  }
  return current;
}
struct resolverEntity *
resolverGetEntityForType(struct resolverResult *result,
                         struct resolverProcess *rprocess,
                         const char *entityName, int entityType) {

  struct resolverScope *scope = rprocess->scope.current;
  struct resolverEntity *entity = NULL;
  while (scope) {
    entity = resolverGetEntityInScopeWithEntityType(result, rprocess, scope,
                                                    entityName, entityType);
    if (entity) {
      break;
    }

    scope = scope->prev;
  }
  if (entity) {
    memset(&entity->lastResolve, 0, sizeof(entity->lastResolve));
  }
  return entity;
}
struct resolverEntity *resolverGetEntity(struct resolverResult *result,
                                         struct resolverProcess *process,
                                         const char *entityName) {
  return resolverGetEntityForType(result, process, entityName, -1);
}

struct resolverEntity *resolverGetEntityInScope(struct resolverResult *result,
                                                struct resolverProcess *process,
                                                struct resolverScope *scope,
                                                const char *entityName) {
  return resolverGetEntityInScopeWithEntityType(result, process, scope,
                                                entityName, -1);
}

struct resolverEntity *resolverGetVariable(struct resolverResult *result,
                                           struct resolverProcess *process,
                                           const char *varName) {
  return resolverGetEntityForType(result, process, varName,
                                  RESOLVER_ENTITY_TYPE_VARIABLE);
}

struct resolverEntity *
resolverGetFunctionInScope(struct resolverResult *result,
                           struct resolverProcess *process,
                           const char *funcName, struct resolverScope *scope) {
  return resolverGetEntityForType(result, process, funcName,
                                  RESOLVER_ENTITY_TYPE_FUNCTION);
}

struct resolverEntity *resolverGetFunction(struct resolverResult *result,
                                           struct resolverProcess *process,
                                           const char *funcName) {
  struct resolverEntity *entity = NULL;
  struct resolverScope *scope = process->scope.root;
  entity = resolverGetFunctionInScope(result, process, funcName, scope);
  return entity;
}
