#pragma once
#include "position.h"
#include <stdbool.h>
#include <stddef.h>

enum {
  DATATYPE_FLAG_IS_SIGNED = 0b00000001,
  DATATYPE_FLAG_IS_STATIC = 0b00000010,
  DATATYPE_FLAG_IS_CONST = 0b00000100,
  DATATYPE_FLAG_IS_POINTER = 0b00001000,
  DATATYPE_FLAG_IS_ARRAY = 0b00010000,
  DATATYPE_FLAG_IS_EXTERN = 0b00100000,
  DATATYPE_FLAG_IS_RESTRICT = 0b01000000,
  DATATYPE_FLAG_IGNORE_TYPE_CHECKING = 0b10000000,
  DATATYPE_FLAG_IS_SECONDARY = 0b100000000,
  DATATYPE_FLAG_STRUCT_UNION_NO_NAME = 0b1000000000,
  DATATYPE_FLAG_IS_LITERAL = 0b10000000000,
};
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
struct arrayBrackets {
  struct vector *nBrackets;
};
struct parsedSwitchedCase {
  int index;
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
  struct array {
    struct arrayBrackets *brackets;
    size_t size;
  } array;
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
    struct parenthesis {
      struct node *exp;
    } parenthesis;
    struct var {
      struct datatype type;
      int padding;
      const char *name;
      int aoffset;
      struct node *val;
    } var;
    struct varlist {
      struct vector *list;
    } varlist;
    struct nodeTenary {
      struct node *trueNode;
      struct node *falseNode;
    } tenary;
    struct bracket {
      struct node *inner;
    } bracket;
    struct _struct {
      const char *name;
      struct node *body_n;
      struct node *var;
    } _struct;
    struct _union {
      const char *name;
      struct node *body_n;
      struct node *var;
    } _union;
    struct body {
      struct vector *statements;
      size_t size;
      bool padded;
      struct node *largestVarNode;
    } body;
    struct function {
      int flags;
      // return data type
      struct datatype rtype;
      // function name
      const char *name;
      struct functionArguments {
        struct vector *vector;
        // how much to add to base ptr to get first argument
        size_t stackAddition;
      } args;
      struct node *bodyN;
      // stack size for all vars inside function
      size_t stackSize;
      struct stackFrame {
        struct vector *elements;
      } frame;
    } func;
    struct statement {
      struct returnStmt {
        struct node *expression;
      } returnStmt;
      struct ifStmt {
        // if(COND)
        struct node *condNode;
        struct node *bodyNode;
        struct node *next;
      } ifStmt;
      struct elseStmt {
        struct node *bodyNode;
      } elseStmt;
      struct forStmt {
        struct node *initNode;
        struct node *condNode;
        struct node *loopNode;
        struct node *bodyNode;
      } forStmt;
      struct whileStmt {
        struct node *expNode;
        struct node *bodyNode;
      } whileStmt;
      struct doWhileStmt {
        struct node *expNode;
        struct node *bodyNode;
      } doWhileStmt;
      struct switchStmt {
        struct node *exp;
        struct node *body;
        struct vector *cases;
        bool hasDefaultCase;
      } switchStmt;
      struct gotoStmt {
        struct node *labelNode;
      } gotoStmt;
      struct caseStmt {
        struct node *exp;
      } _case;
    } stmt;
    struct nodeLabel {
      struct node *name;
    } nodeLabel;
    struct cast {
      struct datatype dtype;
      struct node *operand;
    } cast;
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
