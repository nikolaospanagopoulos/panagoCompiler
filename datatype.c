#include "compiler.h"
#include <string.h>
bool datatypeIsStructOrUnionForName(const char *name) {
  return S_EQ(name, "union") || S_EQ(name, "struct");
}

bool dataTypeIsStructOrUnion(struct datatype *dtype) {
  return dtype->type == DATA_TYPE_STRUCT || DATA_TYPE_UNION;
}
