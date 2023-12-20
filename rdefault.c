#include "compiler.h"
#include "node.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct compileProcess *cp;
void setCompileProcessForResolverDefaultHandler(compileProcess *process) {
  cp = process;
}
struct resolverDefaultEntityData *
resolverDefaultEntityPrivate(struct resolverEntity *entity) {
  return entity->privateData;
}

struct resolverDefaultScopeData *
resolverDefaultScopePrivate(struct resolverScope *scope) {
  return scope->privateData;
}

char *resolverDefaultStackAsmAddress(int stackOffset, char *out) {
  if (stackOffset < 0) {
    sprintf(out, "ebp%i", stackOffset);
    return out;
  }
  sprintf(out, "ebp+%i", stackOffset);
  return out;
}

struct resolverDefaultEntityData *resolverDefaultNewEntityData() {
  struct resolverDefaultEntityData *entityData =
      calloc(sizeof(struct resolverDefaultEntityData), 1);
  // TODO: cleanup
  return entityData;
}

void resolverDefaultGlobalAsmAddress(const char *name, int offset,
                                     char *addressOut) {
  if (offset == 0) {
    sprintf(addressOut, "%s", name);
    return;
  }
  sprintf(addressOut, "%s+%i", name, offset);
}

void resolverDefaultEntityDataSetAddress(
    struct resolverDefaultEntityData *entityData, struct node *varNode,
    int offset, int flags) {
  if (!varNode) {
    return;
  }
  if (!variableNode(varNode)->var.name) {
    return;
  }
  entityData->offset = offset;
  if (flags & RESOLVER_DEFAULT_ENTITY_FLAG_IS_LOCAL_STACK) {
    resolverDefaultStackAsmAddress(offset, entityData->address);
    sprintf(entityData->baseAddress, "ebp");
  } else {
    resolverDefaultGlobalAsmAddress(variableNode(varNode)->var.name, offset,
                                    entityData->address);
    sprintf(entityData->baseAddress, "%s", variableNode(varNode)->var.name);
  }
}
void *resolverDefaultMakePrivate(struct resolverEntity *entity,
                                 struct node *node, int offset,
                                 struct resolverScope *scope) {
  // TODO: cleanup
  struct resolverDefaultEntityData *entityData = resolverDefaultNewEntityData();
  int entityFlags = 0x00;
  if (entity->flags & RESOLVER_ENTITY_FLAG_IS_STACK) {
    entityFlags |= RESOLVER_DEFAULT_ENTITY_FLAG_IS_LOCAL_STACK;
  }
  entityData->offset = offset;
  entityData->flags = entityFlags;
  entityData->type = entity->type;
  if (variableNode(node)) {
    resolverDefaultEntityDataSetAddress(entityData, variableNode(node), offset,
                                        entityFlags);
  }
  return entityData;
}
void resolverDefaultSetResultBase(struct resolverResult *result,
                                  struct resolverEntity *baseEntity) {
  struct resolverDefaultEntityData *entityData =
      resolverDefaultEntityPrivate(baseEntity);
  if (!entityData) {
    return;
  }
  strncpy(result->base.baseAddress, entityData->baseAddress,
          sizeof(result->base.baseAddress));
  strncpy(result->base.address, entityData->address,
          sizeof(result->base.address));
  result->base.offset = entityData->offset;
}
struct resolverDefaultEntityData *
resolverDefaultNewEntityDataForVarNode(struct node *varNode, int offset,
                                       int flags) {
  struct resolverDefaultEntityData *entityData = resolverDefaultNewEntityData();
  if (!variableNode(varNode)) {
    compilerError(cp, "Var node is not set \n");
  }
  entityData->offset = offset;
  entityData->flags = flags;
  entityData->type = RESOLVER_DEFAULT_ENTITY_DATA_TYPE_VARIABLE;
  resolverDefaultEntityDataSetAddress(entityData, variableNode(varNode), offset,
                                      flags);
  return entityData;
}
struct resolverDefaultEntityData *
resolverDefaultNewEntityDataForArrayBracket(struct node *bracketNode) {
  struct resolverDefaultEntityData *entityData = resolverDefaultNewEntityData();
  entityData->type = RESOLVER_DEFAULT_ENTITY_DATA_TYPE_ARRAY_BRACKET;
  return entityData;
}
struct resolverDefaultEntityData *
resolverDefaultNewEntityDataForFunction(struct node *funcNode, int flags) {

  struct resolverDefaultEntityData *entityData = resolverDefaultNewEntityData();
  entityData->flags = flags;
  entityData->type = RESOLVER_DEFAULT_ENTITY_DATA_TYPE_FUNCTION;
  resolverDefaultGlobalAsmAddress(funcNode->func.name, 0, entityData->address);
  return entityData;
}
struct resolverEntity *
resolverDefaultNewScopeEntity(struct resolverProcess *resolver,
                              struct node *varNode, int offset, int flags) {
  if (varNode->type != NODE_TYPE_VARIABLE) {
    compilerError(cp, "Not a variable node \n");
  }
  struct resolverDefaultEntityData *entityData =
      resolverDefaultNewEntityDataForVarNode(variableNode(varNode), offset,
                                             flags);
  // TODO: cleanup
  return resolverNewEntityForVarNode(resolver, variableNode(varNode),
                                     entityData, offset);
}
struct resolverEntity *
resolverDefaultRegisterFunction(struct resolverProcess *resolver,
                                struct node *funcNode, int flags) {
  struct resolverDefaultEntityData *private =
      resolverDefaultNewEntityDataForFunction(funcNode, flags);

  return resolverRegisterFunction(resolver, funcNode, private);
}

void resolverDefaultNewScope(struct resolverProcess *resolver, int flags) {
  struct resolverDefaultScopeData *scopeData =
      calloc(sizeof(struct resolverDefaultScopeData), 1);

  scopeData->flags |= flags;

  resolverNewScope(resolver, scopeData, flags);
}
void resolverDefaultFinishScope(struct resolverProcess *resolver) {
  resolverFinishScope(resolver);
}
void *resolverDefaultNewArrayEntity(struct resolverResult *result,
                                    struct node *arrayEntityNode) {
  return resolverDefaultNewEntityDataForArrayBracket(arrayEntityNode);
}
void resolverDefaultDeleteEntity(struct resolverEntity *entity) {
  free(entity->privateData);
}
void resolverDefaultDeleteScope(struct resolverScope *scope) {
  if (scope && scope->privateData) {
    free(scope->privateData);
  }
}

static void resolverDefaultMergeArrayCalculateOutOffset(
    struct datatype *dtype, struct resolverEntity *entity, int *outOffset) {
  if (entity->array.arrayIndexNode->type != NODE_TYPE_NUMBER) {
    compilerError(cp, "Array index is not of type number \n");
  }
  int indexVal = entity->array.arrayIndexNode->llnum;
  *(outOffset) += arrayOffset(dtype, entity->array.index, indexVal);
}
struct resolverEntity *
resolverDefaultMergeEntities(struct resolverProcess *process,
                             struct resolverResult *result,
                             struct resolverEntity *resolverLeftEntity,
                             struct resolverEntity *resolverRightEntity) {
  int newPos = resolverLeftEntity->offset + resolverRightEntity->offset;
  // TODO: cleanup
  return resolverMakeEntity(
      process, result, &resolverRightEntity->dtype, resolverLeftEntity->node,

      &(struct resolverEntity){.type = resolverRightEntity->type,
                               .flags = resolverLeftEntity->flags,
                               .offset = newPos,
                               .array = resolverRightEntity->array},
      resolverLeftEntity->scope

  );
}
struct resolverProcess *
resolverDefaultNewProcess(struct compileProcess *compileProcess) {

  return resolverNewProcess(
      compileProcess, &(struct resolverCallbacks){
                          .new_array_entity = resolverDefaultNewArrayEntity,
                          .delete_entity = resolverDefaultDeleteEntity,
                          .delete_scope = resolverDefaultDeleteScope,
                          .merge_entities = resolverDefaultMergeEntities,
                          .make_private = resolverDefaultMakePrivate,
                          .set_result_base = resolverDefaultSetResultBase});
}
