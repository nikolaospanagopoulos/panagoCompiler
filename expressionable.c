#include "compiler.h"

struct expressionableOpPrecedenceGroup opPrecedence[TOTAL_OPERATOR_GROUPS] = {
    {.operators = {"++", "--", "()", "[]", "(", "[", ".", "->", NULL},
     .associtivity = ASSOCIATIVITY_LEFT_TO_RIGHT},

    {.operators = {"*", "/", "%", NULL},
     .associtivity = ASSOCIATIVITY_LEFT_TO_RIGHT},

    {.operators = {"+", "-", NULL},
     .associtivity = ASSOCIATIVITY_LEFT_TO_RIGHT},

    {.operators = {"<<", ">>", NULL},
     .associtivity = ASSOCIATIVITY_LEFT_TO_RIGHT},

    {.operators = {"<", "<=", ">", ">=", NULL},
     .associtivity = ASSOCIATIVITY_LEFT_TO_RIGHT},

    {.operators = {"==", "!=", NULL},
     .associtivity = ASSOCIATIVITY_LEFT_TO_RIGHT},

    {.operators = {"&", NULL}, .associtivity = ASSOCIATIVITY_LEFT_TO_RIGHT},
    {.operators = {"^", NULL}, .associtivity = ASSOCIATIVITY_LEFT_TO_RIGHT},
    {.operators = {"|", NULL}, .associtivity = ASSOCIATIVITY_LEFT_TO_RIGHT},
    {.operators = {"&&", NULL}, .associtivity = ASSOCIATIVITY_LEFT_TO_RIGHT},
    {.operators = {"||", NULL}, .associtivity = ASSOCIATIVITY_LEFT_TO_RIGHT},
    {.operators = {"?", ":", NULL},
     .associtivity = ASSOCIATIVITY_RIGHT_TO_LEFT},

    {.operators = {"=", "+=", "-=", "*=", "/=", "%=", "<<=", ">>=", "&=", "^=",
                   "|=", NULL},
     .associtivity = ASSOCIATIVITY_RIGHT_TO_LEFT},
    {.operators = {",", NULL}, .associtivity = ASSOCIATIVITY_LEFT_TO_RIGHT}};
