#pragma once
#include "pos.h"
#include <stdbool.h>
#include <stddef.h>

struct unary {
  // "*" for pointer access.. **** even for multiple pointer access only the
  // first operator is here
  const char *op;
  struct node *operand;
  union {
    struct indirection {
      // The pointer depth
      int depth;
    } indirection;
  };
};

struct datatype {
  int flags;
  // i.e type of long, int, float ect..
  int type;

  // i.e long int. int being the secondary.
  struct datatype *secondary;
  // long
  const char *type_str;
  // The sizeof the datatype.
  size_t size;
  int pointer_depth;

  union {
    struct node *struct_node;
    struct node *union_node;
  };

  struct array {
    struct array_brackets *brackets;
    /**
     *
     * The total array size: Equation = DATATYPE_SIZE * EACH_INDEX
     */
    size_t size;
  } array;
};

struct node

{
  int type;
  int flags;

  struct pos pos;

  struct node_binded {
    // Pointer to our body node
    struct node *owner;

    // Pointer to the function this node is in.
    struct node *function;
  } binded;

  union {
    struct exp {
      struct node *left;
      struct node *right;
      const char *op;
    } exp;

    struct parenthesis {
      // The expression inside the parenthesis node.
      struct node *exp;
    } parenthesis;

    struct var {
      struct datatype type;
      int padding;
      // Aligned offset
      int aoffset;
      const char *name;
      struct node *val;
    } var;

    struct node_tenary {
      struct node *true_node;
      struct node *false_node;
    } tenary;

    struct varlist {
      // A list of struct node* variables.
      struct vector *list;
    } var_list;

    struct bracket {
      // int x[50]; [50] would be our bracket node. The inner would
      // NODE_TYPE_NUMBER value of 50
      struct node *inner;
    } bracket;

    struct _struct {
      const char *name;
      struct node *body_n;

      /**
       * struct abc
       * {
       *
       * } var_name;
       *
       * NULL if no variable attached to structure.
       *
       */
      struct node *var;
    } _struct;

    struct _union {
      const char *name;
      struct node *body_n;

      /**
       * struct abc
       * {
       *
       * } var_name;
       *
       * NULL if no variable attached to structure.
       *
       */
      struct node *var;
    } _union;

    struct body {
      /**
       * struct node* vector of statements
       */
      struct vector *statements;

      // The size of combined variables inside this body.
      size_t size;

      // True if the variable size had to be increased due to padding in the
      // body.
      bool padded;

      // A pointer to the largest variable node in the statements vector.
      struct node *largest_var_node;
    } body;

    struct function {
      // Special flags
      int flags;
      // Return type i.e void, int, long ect...
      struct datatype rtype;

      // I.e function name "main"
      const char *name;

      struct function_arguments {
        // Vector of struct node* . Must be type NODE_TYPE_VARIABLE
        struct vector *vector;

        // How much to add to the EBP to find the first argument.
        size_t stack_addition;
      } args;

      // Pointer to the function body node, NULL if this is a function prototype
      struct node *body_n;

      struct stack_frame {
        // A vector of stack_frame_element
        struct vector *elements;
      } frame;

      // The stack size for all variables inside this function.
      size_t stack_size;
    } func;

    struct statement {

      struct return_stmt {
        // The expression of the return
        struct node *exp;
      } return_stmt;

      struct if_stmt {
        // if(COND) {// body }
        struct node *cond_node;
        struct node *body_node;

        // if(COND) {} else {}
        struct node *next;
      } if_stmt;

      struct else_stmt {
        struct node *body_node;
      } else_stmt;

      struct for_stmt {
        struct node *init_node;
        struct node *cond_node;
        struct node *loop_node;
        struct node *body_node;
      } for_stmt;

      struct while_stmt {
        struct node *exp_node;
        struct node *body_node;
      } while_stmt;

      struct do_while_stmt {
        struct node *exp_node;
        struct node *body_node;
      } do_while_stmt;

      struct switch_stmt {
        struct node *exp;
        struct node *body;
        struct vector *cases;
        bool has_default_case;
      } switch_stmt;

      struct _case_stmt {
        struct node *exp;
      } _case;

      struct _goto_stmt {
        struct node *label;
      } _goto;
    } stmt;

    struct node_label {
      struct node *name;
    } label;

    struct cast {
      struct datatype dtype;
      struct node *operand;
    } cast;

    struct unary unary;
  };

  union {
    char cval;
    const char *sval;
    unsigned int inum;
    unsigned long lnum;
    unsigned long long llnum;
  };
};
