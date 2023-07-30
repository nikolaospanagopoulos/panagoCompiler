#pragma once

#include "compiler.h"

struct fixup;
typedef bool (*FIXUP_FIX)(struct fixup *fixup);
typedef void (*FIXUP_END)(struct fixup *fixup);
struct fixupConfig {
  FIXUP_FIX fix;
  FIXUP_END end;
  void *privateData;
};
struct fixupSystem {
  struct vector *fixups;
};
enum { FIXUP_RESOLVED_FLAG = 0b00000001 };
struct fixup {
  int flags;
  struct fixupSystem *system;
  struct fixupConfig config;
};

struct fixupSystem *fixupSystemNew();
struct fixupConfig *fixupConfig(struct fixup *fixup);
void fixupFree(struct fixup *fixup);
struct fixup *fixupNext(struct fixupSystem *system);
void fixupSystemFixupsFree(struct fixupSystem *system);
void fixupSystemFree(struct fixupSystem *system);
int fixupSystemUnresolvedFixupsCount(struct fixupSystem *system);
struct fixup *fixupRegister(struct fixupSystem *system,
                            struct fixupConfig *config);
bool fixupsResolve(struct fixupSystem *system);
void *fixupPrivate(struct fixup *fixup);
bool fixupResolve(struct fixup *fixup);
