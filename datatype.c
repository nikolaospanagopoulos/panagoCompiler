#include "compiler.h"
#include <string.h>
bool datatypeIsStructOrUnionForName(const char *name) {
  return S_EQ(name, "union") || S_EQ(name, "struct");
}
