#include "compileProcess.h"
#include "compiler.h"
#include "node.h"
#include "vector.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static struct compileProcess *cp;
void resolverFollowPart(struct resolverProcess *resolver, struct node *node,
                        struct resolverResult *result);
void resolverResultEntityPush(struct resolverResult *result,
                              struct resolverEntity *entity);

void resolverNewEntityForRule(struct resolverProcess *process,
                              struct resolverResult *result,
                              struct resolverEntityRule *rule);
struct resolverEntity *resolverFollowExp(struct resolverProcess *resolver,
                                         struct node *node,
                                         struct resolverResult *result);
struct resolverResult *resolverFollow(struct resolverProcess *resolver,
                                      struct node *node);
struct resolverEntity *
resolverFollowPartReturnEntity(struct resolverProcess *process,
                               struct node *node,
                               struct resolverResult *result);

void resolverFinalizeLastEntity(struct resolverProcess *process,
                                struct resolverResult *result);
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
  vector_push(cp->trackedScopes, &scope);
  return scope;
}
struct resolverScope *resolverNewScope(struct resolverProcess *resolver,
                                       void *privateData, int flags) {
  struct resolverScope *scope = resolverNewScopeCreate();
  if (!scope) {
    return NULL;
  }
  resolver->scope.current->next = scope;
  scope->prev = resolver->scope.current;
  resolver->scope.current = scope;
  scope->privateData = privateData;
  scope->flags = flags;
  return scope;
}
void resolverFinishScope(struct resolverProcess *resolver) {
  struct resolverScope *scope = resolver->scope.current;
  resolver->scope.current = scope->prev;
}

struct resolverProcess *
resolverNewProcess(struct compileProcess *compiler,
                   struct resolverCallbacks *callbacks) {
  struct resolverProcess *process = calloc(1, sizeof(struct resolverProcess));
  compiler->trackedScopes = vector_create(sizeof(struct resolverScope *));
  process->cmpProcess = compiler;
  cp = compiler;
  memcpy(&process->callbacks, callbacks, sizeof(process->callbacks));
  process->scope.root = resolverNewScopeCreate();
  process->scope.current = process->scope.root;
  return process;
}

struct resolverEntity *resolverCreateNewEntity(struct resolverResult *result,
                                               int type, void *private) {
  // TODO: cleanup
  struct resolverEntity *entity = calloc(1, sizeof(struct resolverEntity));
  if (!entity) {
    return NULL;
  }
  entity->type = type;
  entity->privateData = private;
  vector_push(cp->gbVectorForCustonResolverEntities, &entity);

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

struct resolverEntity *resolverFollowForName(struct resolverProcess *resolver,
                                             const char *name,
                                             struct resolverResult *result) {
  struct resolverEntity *entity =
      resolverEntityClone(resolverGetEntity(result, resolver, name));
  if (!entity) {
    return NULL;
  }
  resolverResultEntityPush(result, entity);
  if (!result->identifier) {
    result->identifier = entity;
  }
  if ((entity->type == RESOLVER_ENTITY_TYPE_VARIABLE &&
       dataTypeIsStructOrUnion(&entity->varData.dtype)) ||

      (entity->type == RESOLVER_ENTITY_TYPE_FUNCTION &&
       dataTypeIsStructOrUnion(&entity->dtype))) {
    result->lastStructUnionEntity = entity;
  }
  return entity;
}
struct resolverEntity *
resolverFollowIdentifier(struct resolverProcess *resolver, struct node *node,
                         struct resolverResult *result) {
  struct resolverEntity *entity =
      resolverFollowForName(resolver, node->sval, result);
  if (entity) {
    entity->lastResolve.referencingNode = node;
  }
  return entity;
}
struct resolverEntity *resolverFollowVariable(struct resolverProcess *process,
                                              struct node *node,
                                              struct resolverResult *result) {
  struct resolverEntity *entity =
      resolverFollowForName(process, node->var.name, result);
  return entity;
}
struct resolverEntity *
resolverFollowStructExpression(struct resolverProcess *resolver,
                               struct node *node,
                               struct resolverResult *result) {
  struct resolverEntity *resultEntity = NULL;
  resolverFollowPart(resolver, node->exp.left, result);
  struct resolverEntity *leftEntity = resolverResultPeek(result);
  struct resolverEntityRule rule = {};
  if (isAccessNodeWithOp(node, "->")) {
    rule.left.flags = RESOLVER_ENTITY_FLAG_NO_MERGE_WITH_NEXT_ENTITY;
    if (leftEntity->type != RESOLVER_ENTITY_TYPE_FUNCTION_CALL) {
      rule.right.flags = RESOLVER_ENTITY_FLAG_DO_INDIRECTION;
    }
  }
  resolverNewEntityForRule(resolver, result, &rule);
  resolverFollowPart(resolver, node->exp.right, result);
  return NULL;
}
struct resolverEntity *resolverFollowArray(struct resolverProcess *resolver,
                                           struct node *node,
                                           struct resolverResult *result) {
  resolverFollowPart(resolver, node->exp.left, result);
  struct resolverEntity *leftEntity = resolverResultPeek(result);
  resolverFollowPart(resolver, node->exp.right, result);
  return leftEntity;
}
struct datatype *resolverGetDatatype(struct resolverProcess *resolver,
                                     struct node *node) {
  struct resolverResult *result = resolverFollow(resolver, node);
  if (!resolverResultOK(result)) {
    return NULL;
  }
  return &result->lastEntity->dtype;
}
void resolverBuildFunctionCallArguments(
    struct resolverProcess *resolver, struct node *argumentNode,
    struct resolverEntity *rootFuncCallEntity, size_t *totalSizeOut) {
  if (isArgumentNode(argumentNode)) {
    resolverBuildFunctionCallArguments(resolver, argumentNode->exp.left,
                                       rootFuncCallEntity, totalSizeOut);
    resolverBuildFunctionCallArguments(resolver, argumentNode->exp.right,
                                       rootFuncCallEntity, totalSizeOut);
  } else if (argumentNode->type == NODE_TYPE_EXPRESSION_PARENTHESES) {
    resolverBuildFunctionCallArguments(resolver, argumentNode->parenthesis.exp,
                                       rootFuncCallEntity, totalSizeOut);
  } else if (nodeValid(argumentNode)) {
    vector_push(rootFuncCallEntity->funcCallData.arguments, &argumentNode);
    size_t stackChange = DATA_SIZE_DWORD;
    struct datatype *dtype = resolverGetDatatype(resolver, argumentNode);
    if (dtype) {
      stackChange = datatypeElementSize(dtype);
      if (stackChange < DATA_SIZE_DWORD) {
        stackChange = DATA_SIZE_DWORD;
      }
      stackChange = alignValue(stackChange, DATA_SIZE_DWORD);
    }
    *totalSizeOut += stackChange;
  }
}
struct resolverEntity *
resolverFollowFunctionCall(struct resolverProcess *resolver, struct node *node,
                           struct resolverResult *result) {
  // HERE
  resolverFollowPart(resolver, node->exp.left, result);
  struct resolverEntity *leftEntity = resolverResultPeek(result);
  struct resolverEntity *funcCallEntity =
      resolverCreateNewEntityForFunctionCall(result, resolver, leftEntity,
                                             NULL);
  if (!funcCallEntity) {
    compilerError(
        cp, "There was something wrong creating the function call entity \n");
  }
  funcCallEntity->flags |= RESOLVER_ENTITY_FLAG_NO_MERGE_WITH_NEXT_ENTITY |
                           RESOLVER_ENTITY_FLAG_NO_MERGE_WITH_LEFT_ENTITY;

  resolverBuildFunctionCallArguments(resolver, node->exp.right, funcCallEntity,
                                     &funcCallEntity->funcCallData.stackSize);
  resolverResultEntityPush(result, funcCallEntity);
  return funcCallEntity;
}
struct resolverEntity *
resolverFollowParentheses(struct resolverProcess *resolver, struct node *node,
                          struct resolverResult *result) {
  if (node->exp.left->type == NODE_TYPE_IDENTIFIER) {
    return resolverFollowFunctionCall(resolver, node, result);
  }
  return resolverFollowExp(resolver, node->parenthesis.exp, result);
}
struct resolverEntity *resolverFollowExp(struct resolverProcess *resolver,
                                         struct node *node,
                                         struct resolverResult *result) {
  struct resolverEntity *entity = NULL;
  if (isAccessNode(node)) {
    entity = resolverFollowStructExpression(resolver, node, result);
  } else if (isArrayNode(node)) {
    entity = resolverFollowArray(resolver, node, result);
  } else if (isParenthesesNode(node)) {
    entity = resolverFollowParentheses(resolver, node, result);
  }
  return entity;
}
void resolverArrayBracketSetFlags(struct resolverEntity *bracketEntity,
                                  struct datatype *dtype,
                                  struct node *bracketNode, int index) {
  if (!(dtype->flags & DATATYPE_FLAG_IS_ARRAY) ||
      arrayBracketsCount(dtype) <= index) {
    bracketEntity->flags = RESOLVER_ENTITY_FLAG_NO_MERGE_WITH_LEFT_ENTITY |
                           RESOLVER_ENTITY_FLAG_NO_MERGE_WITH_NEXT_ENTITY |
                           RESOLVER_ENTITY_FLAG_IS_POINTER_ARRAY_ENTITY;
  } else if (bracketNode->bracket.inner->type != NODE_TYPE_NUMBER) {
    bracketEntity->flags = RESOLVER_ENTITY_FLAG_NO_MERGE_WITH_LEFT_ENTITY |
                           RESOLVER_ENTITY_FLAG_NO_MERGE_WITH_NEXT_ENTITY;
  } else {
    bracketEntity->flags = RESOLVER_ENTITY_FLAG_JUST_USE_OFFSET;
  }
}
struct resolverEntity *
resolverFollowArrayBracket(struct resolverProcess *process, struct node *node,
                           struct resolverResult *result) {
  if (node->type != NODE_TYPE_BRACKET) {
    compilerError(cp, "Not a bracket node");
  }
  int index = 0;
  struct datatype dtype;
  struct resolverScope *scope = NULL;
  struct resolverEntity *lastEntity =
      resolverResultPeekIgnoreRuleEntity(result);
  scope = lastEntity->scope;
  dtype = lastEntity->dtype;
  if (lastEntity->type == RESOLVER_ENTITY_TYPE_ARRAY_BRACKET) {
    index = lastEntity->array.index + 1;
  }
  if (dtype.flags & DATATYPE_FLAG_IS_ARRAY) {
    dtype.array.size = arrayBracketsCalculateSizeFromIndex(
        &dtype, dtype.array.brackets, index + 1);
  }
  void *private = process->callbacks.new_array_entity(result, node);
  struct resolverEntity *arrayBracketEntity =
      resolverCreateNewEntityForArrayBracket(result, process, node,
                                             node->bracket.inner, index, &dtype,
                                             private, scope);

  struct resolverEntityRule rule = {};
  resolverArrayBracketSetFlags(arrayBracketEntity, &dtype, node, index);
  lastEntity->flags |= RESOLVER_ENTITY_FLAG_USES_ARRAY_BRACKETS;
  if (arrayBracketEntity->flags &
      RESOLVER_ENTITY_FLAG_IS_POINTER_ARRAY_ENTITY) {
    datatypeDecrementPtr(&arrayBracketEntity->dtype);
  }
  resolverResultEntityPush(result, arrayBracketEntity);
  return arrayBracketEntity;
}
struct resolverEntity *
resolverFollowExpParentheses(struct resolverProcess *process, struct node *node,
                             struct resolverResult *result) {
  return resolverFollowPartReturnEntity(process, node->parenthesis.exp, result);
}

struct resolverEntity *
resolverFollowUnsupportedUnaryNode(struct resolverProcess *process,
                                   struct node *node,
                                   struct resolverResult *result) {
  return resolverFollowPartReturnEntity(process, node->parenthesis.exp, result);
}
struct resolverEntity *
resolverFollowUnsupportedNode(struct resolverProcess *process,
                              struct node *node,
                              struct resolverResult *result) {
  bool followed = false;

  switch (node->type) {

  case NODE_TYPE_UNARY:
    resolverFollowUnsupportedUnaryNode(process, node, result);
    followed = true;
    break;

  default:
    followed = false;
  }

  struct resolverEntity *unsupportedEntity =
      resolverCreateNewEntityForUnsupportedNodes(result, node);

  if (!unsupportedEntity) {
    compilerError(cp, "Unsupported entity doesnt exist \n");
  }
  resolverResultEntityPush(result, unsupportedEntity);
  return unsupportedEntity;
}

struct resolverEntity *resolverFollowCast(struct resolverProcess *process,
                                          struct node *node,
                                          struct resolverResult *result) {
  struct resolverEntity *operandEntity = NULL;
  resolverFollowUnsupportedNode(process, node->cast.operand, result);

  operandEntity = resolverResultPeek(result);
  operandEntity->flags |= RESOLVER_ENTITY_FLAG_WAS_CASTED;

  struct resolverEntity *castEntity = resolverCreateNewCastEntity(
      process, operandEntity->scope, &node->cast.dtype);

  resolverResultEntityPush(result, castEntity);

  return castEntity;
}
struct resolverEntity *
resolverFollowIndirection(struct resolverProcess *process, struct node *node,
                          struct resolverResult *result) {
  resolverFollowPart(process, node->unary.operand, result);

  struct resolverEntity *lastEntity = resolverResultPeek(result);

  if (!lastEntity) {
    lastEntity =
        resolverFollowUnsupportedNode(process, node->unary.operand, result);
  }
  struct resolverEntity *unaryIndirectionEntity =
      resolverCreateNewUnaryIndirectionEntity(process, result, node,
                                              node->unary.indirection.depth);

  resolverResultEntityPush(result, unaryIndirectionEntity);

  return unaryIndirectionEntity;
}
struct resolverEntity *
resolverFollowUnaryAddress(struct resolverProcess *process, struct node *node,
                           struct resolverResult *result) {
  resolverFollowPart(process, node->unary.operand, result);
  struct resolverEntity *lastEntity = resolverResultPeek(result);
  struct resolverEntity *unaryAddressEntity =
      resolverCreateNewUnaryGetAddressEntity(
          process, result, &lastEntity->dtype, node, lastEntity->scope,
          lastEntity->offset);

  resolverResultEntityPush(result, unaryAddressEntity);

  return unaryAddressEntity;
}
struct resolverEntity *resolverFollowUnary(struct resolverProcess *process,
                                           struct node *node,
                                           struct resolverResult *result) {
  struct resolverEntity *resultEntity = NULL;
  if (opIsIndirection(node->unary.op)) {
    resultEntity = resolverFollowIndirection(process, node, result);
  } else if (opIsAddress(node->unary.op)) {
    resultEntity = resolverFollowUnaryAddress(process, node, result);
  }
  return resultEntity;
}
struct resolverEntity *
resolverFollowPartReturnEntity(struct resolverProcess *process,
                               struct node *node,
                               struct resolverResult *result) {
  struct resolverEntity *entity = NULL;
  switch (node->type) {
  case NODE_TYPE_IDENTIFIER:
    entity = resolverFollowIdentifier(process, node, result);
    break;
  case NODE_TYPE_VARIABLE:
    entity = resolverFollowVariable(process, node, result);
    break;
  case NODE_TYPE_EXPRESSION:
    entity = resolverFollowExp(process, node, result);
    break;
  case NODE_TYPE_BRACKET:
    entity = resolverFollowArrayBracket(process, node, result);
    break;
  case NODE_TYPE_EXPRESSION_PARENTHESES:
    entity = resolverFollowExpParentheses(process, node, result);
    break;
  case NODE_TYPE_CAST:
    entity = resolverFollowCast(process, node, result);
    break;
  case NODE_TYPE_UNARY:
    entity = resolverFollowUnary(process, node, result);
    break;
  default: {
    entity = resolverFollowUnsupportedNode(process, node, result);
  }
  }
  if (entity) {
    entity->result = result;
    entity->process = process;
  }
  return entity;
}
void resolverFollowPart(struct resolverProcess *resolver, struct node *node,
                        struct resolverResult *result) {
  resolverFollowPartReturnEntity(resolver, node, result);
}
void resolverMergeCompileTimes(struct resolverProcess *resolver,
                               struct resolverResult *result) {}

void resolverFinalizeResultFlags(struct resolverProcess *process,
                                 struct resolverResult *result) {
  int flags = RESOLVER_RESULT_FLAG_FIRST_ENTITY_PUSH_VALUE;
  struct resolverEntity *entity = result->entity;
  struct resolverEntity *firstEntity = entity;
  struct resolverEntity *lastEntity = result->lastEntity;

  bool doesGetAddress = false;

  if (entity == lastEntity) {
    if (lastEntity->type == RESOLVER_ENTITY_TYPE_VARIABLE &&
        datatypeIsStructOrUnionNotPtr(&lastEntity->dtype)) {
      flags |= RESOLVER_RESULT_FLAG_FIRST_ENTITY_LOAD_TO_EBX;
      flags &= ~RESOLVER_RESULT_FLAG_FIRST_ENTITY_PUSH_VALUE;
    }

    result->flags = flags;
    return;
  }
  while (entity) {
    if (entity->flags & RESOLVER_ENTITY_FLAG_DO_INDIRECTION) {
      flags |= RESOLVER_RESULT_FLAG_FIRST_ENTITY_LOAD_TO_EBX |
               RESOLVER_RESULT_FLAG_FINAL_INDIRECTION_REQUIRED_FOR_VALUE;
      flags &= ~RESOLVER_RESULT_FLAG_FIRST_ENTITY_PUSH_VALUE;
    }
    if (entity->type == RESOLVER_ENTITY_TYPE_UNARY_GET_ADDRESS) {
      flags |= RESOLVER_RESULT_FLAG_FIRST_ENTITY_LOAD_TO_EBX |
               RESOLVER_RESULT_FLAG_DOES_GET_ADDRESS;
      flags &= ~RESOLVER_RESULT_FLAG_FIRST_ENTITY_PUSH_VALUE |
               RESOLVER_RESULT_FLAG_FINAL_INDIRECTION_REQUIRED_FOR_VALUE;
      doesGetAddress = true;
    }
    if (entity->type == RESOLVER_ENTITY_TYPE_FUNCTION_CALL) {
      flags |= RESOLVER_RESULT_FLAG_FIRST_ENTITY_LOAD_TO_EBX;
      flags &= ~RESOLVER_RESULT_FLAG_FIRST_ENTITY_PUSH_VALUE;
    }

    if (entity->type == RESOLVER_ENTITY_TYPE_ARRAY_BRACKET) {
      if (entity->dtype.flags & DATATYPE_FLAG_IS_POINTER) {
        flags |= RESOLVER_RESULT_FLAG_FIRST_ENTITY_PUSH_VALUE;
        flags &= ~RESOLVER_RESULT_FLAG_FIRST_ENTITY_LOAD_TO_EBX;
      } else {
        flags |= RESOLVER_RESULT_FLAG_FIRST_ENTITY_LOAD_TO_EBX;
        flags &= ~RESOLVER_RESULT_FLAG_FIRST_ENTITY_PUSH_VALUE;
      }

      if (entity->flags & RESOLVER_ENTITY_FLAG_IS_POINTER_ARRAY_ENTITY) {
        flags |= RESOLVER_RESULT_FLAG_FINAL_INDIRECTION_REQUIRED_FOR_VALUE;
      }
    }
    if (entity->type == RESOLVER_ENTITY_TYPE_GENERAL) {
      flags |= RESOLVER_RESULT_FLAG_FIRST_ENTITY_LOAD_TO_EBX |
               RESOLVER_RESULT_FLAG_FINAL_INDIRECTION_REQUIRED_FOR_VALUE;
      flags &= ~RESOLVER_RESULT_FLAG_FIRST_ENTITY_PUSH_VALUE;
    }
    entity = entity->next;
  }
  if (lastEntity->dtype.flags & DATATYPE_FLAG_IS_ARRAY &&
      (!doesGetAddress && lastEntity->type == RESOLVER_ENTITY_TYPE_VARIABLE &&
       !(lastEntity->flags & RESOLVER_ENTITY_FLAG_USES_ARRAY_BRACKETS))) {
    flags &= ~RESOLVER_RESULT_FLAG_FINAL_INDIRECTION_REQUIRED_FOR_VALUE;
  } else if (lastEntity->type == RESOLVER_ENTITY_TYPE_VARIABLE) {
    flags |= RESOLVER_RESULT_FLAG_FINAL_INDIRECTION_REQUIRED_FOR_VALUE;
  }
  if (doesGetAddress) {
    flags &= ~RESOLVER_RESULT_FLAG_FINAL_INDIRECTION_REQUIRED_FOR_VALUE;
  }
  result->flags |= flags;
}

void resolverFinalizeResult(struct resolverProcess *resolver,
                            struct resolverResult *result) {

  struct resolverEntity *firstEntity = resolverResultEntityRoot(result);
  if (!firstEntity) {
    return;
  }
  resolver->callbacks.set_result_base(result, firstEntity);
  resolverFinalizeResultFlags(resolver, result);
  resolverFinalizeLastEntity(resolver, result);
}

void resolverFinalizeUnary(struct resolverProcess *process,
                           struct resolverResult *result,
                           struct resolverEntity *entity) {
  struct resolverEntity *previousEntity = entity->prev;
  if (!previousEntity) {
    return;
  }
  entity->scope = previousEntity->scope;
  entity->dtype = previousEntity->dtype;
  entity->offset = previousEntity->offset;

  if (entity->type == RESOLVER_ENTITY_TYPE_UNARY_INDIRECTION) {
    int indirectionDepth = entity->indirection.depth;
    entity->dtype.ptrDepth -= indirectionDepth;
    if (entity->dtype.ptrDepth <= 0) {
      entity->dtype.flags &= ~DATATYPE_FLAG_IS_POINTER;
    }
  } else if (entity->type == RESOLVER_ENTITY_TYPE_UNARY_GET_ADDRESS) {
    entity->dtype.flags |= DATATYPE_FLAG_IS_POINTER;
    entity->dtype.ptrDepth++;
  }
}
void resolverFinalizeLastEntity(struct resolverProcess *process,
                                struct resolverResult *result) {
  struct resolverEntity *lastEntity = resolverResultPeek(result);

  switch (lastEntity->type) {

  case RESOLVER_ENTITY_TYPE_UNARY_INDIRECTION:
  case RESOLVER_ENTITY_TYPE_UNARY_GET_ADDRESS:
    resolverFinalizeUnary(process, result, lastEntity);
    break;
  }
}

void resolverRuleApplyRules(struct resolverEntity *ruleEntity,
                            struct resolverEntity *leftEntity,
                            struct resolverEntity *rightEntity) {
  if (ruleEntity->type != RESOLVER_ENTITY_TYPE_RULE) {
    compilerError(cp, "No rule entity \n");
  }
  if (leftEntity) {
    leftEntity->flags |= ruleEntity->rule.left.flags;
  }
  if (rightEntity) {
    rightEntity->flags |= ruleEntity->rule.right.flags;
  }
}

void resolverPushVectorOfEntities(struct resolverResult *result,
                                  struct vector *vec) {
  vector_set_peek_pointer_end(vec);

  vector_set_flag(vec, VECTOR_FLAG_PEEK_DECREMENT);

  struct resolverEntity *entity = vector_peek_ptr(vec);

  while (entity) {
    resolverResultEntityPush(result, entity);
    entity = vector_peek_ptr(vec);
  }
}

void resolverExecuteRules(struct resolverProcess *resolver,
                          struct resolverResult *result) {

  struct vector *savedEntities = vector_create(sizeof(struct resolverEntity *));
  // TODO: CHECK MEMORY
  vector_push(cp->gbForVectors, savedEntities);
  struct resolverEntity *entity = resolverResultPop(result);
  struct resolverEntity *lastProcessedEntity = NULL;

  while (entity) {
    if (entity->type == RESOLVER_ENTITY_TYPE_RULE) {
      struct resolverEntity *leftEntity = resolverResultPop(result);
      resolverRuleApplyRules(entity, leftEntity, lastProcessedEntity);
      entity = leftEntity;
    }
    vector_push(savedEntities, &entity);
    lastProcessedEntity = entity;
    entity = resolverResultPop(result);
  }

  resolverPushVectorOfEntities(result, savedEntities);
}
struct resolverResult *resolverFollow(struct resolverProcess *resolver,
                                      struct node *node) {
  if (!resolver) {
    compilerError(cp, "Resolver doesn't exist \n");
  }
  if (!node) {
    compilerError(cp, "Node to resolve doesn't exist\n");
  }
  struct resolverResult *result = resolverNewResult(resolver);
  resolverFollowPart(resolver, node, result);
  if (!resolverResultEntityRoot(result)) {
    result->flags |= RESOLVER_RESULT_FLAG_FAILED;
  }
  resolverExecuteRules(resolver, result);
  resolverMergeCompileTimes(resolver, result);
  resolverFinalizeResult(resolver, result);
  return result;
}
