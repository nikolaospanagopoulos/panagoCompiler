#include "compiler.h"

struct resolverDefaultEntityData *
resolverDefaultEntityPrivate(struct resolverEntity *entity) {
  return entity->privateData;
}
