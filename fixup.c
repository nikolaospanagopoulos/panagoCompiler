#include "fixup.h"
#include "vector.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
struct fixupSystem *fixupSystemNew() {
  struct fixupSystem *fxSystem = calloc(1, sizeof(struct fixupSystem));
  fxSystem->fixups = vector_create(sizeof(struct fixup));
  return fxSystem;
}
struct fixupConfig *fixupConfig(struct fixup *fixup) { return &fixup->config; }
void fixupFree(struct fixup *fixup) {
  fixup->config.end(fixup);
  free(fixup);
}
void fixupStartIteration(struct fixupSystem *system) {
  vector_set_peek_pointer(system->fixups, 0);
}

struct fixup *fixupNext(struct fixupSystem *system) {
  return vector_peek_ptr(system->fixups);
}

void fixupSystemFixupsFree(struct fixupSystem *system) {
  fixupStartIteration(system);
  struct fixup *fixup = fixupNext(system);
  while (fixup) {
    fixupFree(fixup);
    fixup = fixupNext(system);
  }
}

void fixupSystemFree(struct fixupSystem *system) {
  fixupSystemFixupsFree(system);
  vector_free(system->fixups);
  free(system);
}

int fixupSystemUnresolvedFixupsCount(struct fixupSystem *system) {
  size_t c = 0;
  fixupStartIteration(system);
  struct fixup *fixup = fixupNext(system);
  while (fixup) {
    if (fixup->flags & FIXUP_RESOLVED_FLAG) {
      fixup = fixupNext(system);
      continue;
    }
    c++;
    fixup = fixupNext(system);
  }
  return c;
}
struct fixup *fixupRegister(struct fixupSystem *system,
                            struct fixupConfig *config) {
  struct fixup *fixup = calloc(1, sizeof(struct fixup));
  memcpy(&fixup->config, config, sizeof(struct fixupConfig));
  fixup->system = system;
  // TODO: maybe have to clean
  vector_push(system->fixups, fixup);
  return fixup;
}
void *fixupPrivate(struct fixup *fixup) {
  return fixupConfig(fixup)->privateData;
}
bool fixupResolve(struct fixup *fixup) {
  if (fixupConfig(fixup)->fix(fixup)) {
    fixup->flags |= FIXUP_RESOLVED_FLAG;
    return true;
  }
  return false;
}
bool fixupsResolve(struct fixupSystem *system) {
  fixupStartIteration(system);
  struct fixup *fixup = fixupNext(system);
  while (fixup) {
    if (fixup->flags & FIXUP_RESOLVED_FLAG) {
      continue;
    }
    fixupResolve(fixup);
    fixup = fixupNext(system);
  }
  return fixupSystemUnresolvedFixupsCount(system) == 0;
}
