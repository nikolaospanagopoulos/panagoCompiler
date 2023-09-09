#include "compileProcess.h"
#include "compiler.h"
#include "vector.h"
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
