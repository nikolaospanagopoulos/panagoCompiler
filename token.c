#include "token.h"
#include "compiler.h"
#include <stdbool.h>
#include <string.h>
bool tokenIsKeyword(token *token, const char *value) {
  return token->type == KEYWORD && S_EQ(token->sval, value);
}
