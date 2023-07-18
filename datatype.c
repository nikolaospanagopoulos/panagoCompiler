#include "compiler.h"
#include "node.h"
#include <stddef.h>
#include <string.h>
bool datatypeIsStructOrUnionForName(const char *name) {
  return S_EQ(name, "union") || S_EQ(name, "struct");
}

bool dataTypeIsStructOrUnion(struct datatype *dtype) {
  return dtype->type == DATA_TYPE_STRUCT || DATA_TYPE_UNION;
}

size_t datatypeElementSize(struct datatype *dtype) {
  if (dtype->flags & DATATYPE_FLAG_IS_POINTER) {
    return DATA_SIZE_DWORD;
  }
  return dtype->size;
}

size_t datatypeSizeForArrayAccess(struct datatype *dtype) {
  if (dataTypeIsStructOrUnion(dtype) &&
      dtype->flags & DATATYPE_FLAG_IS_POINTER && dtype->ptrDepth == 1) {
    return dtype->size;
  }
  return dtype->size;
}

size_t datatypeSizeNoPtr(struct datatype *dtype) {
  if (dtype->flags & DATATYPE_FLAG_IS_ARRAY) {
    return dtype->array.size;
  }
  return dtype->size;
}

size_t datatypeSize(struct datatype *dtype) {
  if (dtype->flags & DATATYPE_FLAG_IS_POINTER && dtype->ptrDepth > 0) {
    return DATA_SIZE_DWORD;
  }
  if (dtype->flags & DATATYPE_FLAG_IS_ARRAY) {
    return dtype->array.size;
  }
  return dtype->size;
}
bool datatypeIsPrimitive(struct datatype *dtype) {
  return !dataTypeIsStructOrUnion(dtype);
}
