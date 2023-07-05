#include "token.h"
#include "compiler.h"
#include <stdbool.h>
#include <string.h>
#define PRIMITIVE_TYPES_NUM 7

const char *primitiveTypes[PRIMITIVE_TYPES_NUM] = {
    "void", "char", "short", "int", "long", "float", "double"};

bool tokenIsKeyword(token *token, const char *value) {
  return token->type == KEYWORD && S_EQ(token->sval, value);
}

bool tokenIsPrimitiveKeyword(token *token) {
  if (token->type != KEYWORD) {
    return false;
  }
  for (int i = 0; i < PRIMITIVE_TYPES_NUM; i++) {
    if (S_EQ(primitiveTypes[i], token->sval)) {
      return true;
    }
  }
  return false;
}

bool tokenIsOperator(token *token, const char *val) {
  return token->type == OPERATOR && S_EQ(token->sval, val);
}
