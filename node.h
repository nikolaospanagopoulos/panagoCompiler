#pragma once
#include "datatype.h"
#include "position.h"
#include <stddef.h>

enum {
  NODE_TYPE_EXPRESSION,
  NODE_TYPE_EXPRESSION_PARENTHESES,
  NODE_TYPE_NUMBER,
  NODE_TYPE_IDENTIFIER,
  NODE_TYPE_STRING,
  NODE_TYPE_VARIABLE,
  NODE_TYPE_VARIABLE_LIST,
  NODE_TYPE_FUNCTION,
  NODE_TYPE_BODY,
  NODE_TYPE_STATEMENT_RETURN,
  NODE_TYPE_STATEMENT_IF,
  NODE_TYPE_STATEMENT_ELSE,
  NODE_TYPE_STATEMENT_WHILE,
  NODE_TYPE_STATEMENT_DO_WHILE,
  NODE_TYPE_STATEMENT_FOR,
  NODE_TYPE_STATEMENT_BREAK,
  NODE_TYPE_STATEMENT_CONTINUE,
  NODE_TYPE_STATEMENT_SWITCH,
  NODE_TYPE_STATEMENT_CASE,
  NODE_TYPE_STATEMENT_DEFAULT,
  NODE_TYPE_STATEMENT_GOTO,

  NODE_TYPE_UNARY,
  NODE_TYPE_TENARY,
  NODE_TYPE_LABEL,
  NODE_TYPE_STRUCT,
  NODE_TYPE_UNION,
  NODE_TYPE_BRACKET,
  NODE_TYPE_CAST,
  NODE_TYPE_BLANK
};
struct node;
typedef struct datatype {
  int flags;
  int type;
  struct datatype *secondary;
  const char *typeStr;
  size_t size;
  int ptrDepth;
  union {
    struct node *structNode;
    struct node *unionNode;
  };
} datatype;
typedef struct node {
  int type;
  int flags;
  pos position;
  struct nodeBinded {
    struct node *owner;
    struct node *function;
  } binded;
  union {
    struct exp {
      struct node *left;
      struct node *right;
      const char *op;
    } exp;
    struct var {
      struct datatype type;
      const char *name;
      struct node *val;
    } var;
  };

  union {
    char cval;
    char *sval;
    unsigned int inum;
    unsigned long lnum;
    unsigned long long llnum;
    void *any;
  };
} node;
