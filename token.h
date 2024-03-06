#pragma once
#include "pos.h"
#include <stdbool.h>
enum {
  TOKEN_TYPE_IDENTIFIER,
  TOKEN_TYPE_KEYWORD,
  TOKEN_TYPE_OPERATOR,
  TOKEN_TYPE_SYMBOL,
  TOKEN_TYPE_NUMBER,
  TOKEN_TYPE_STRING,
  TOKEN_TYPE_COMMENT,
  TOKEN_TYPE_NEWLINE
};
struct token {
  int type;
  int flags;
  struct pos pos;
  union {
    char cval;
    const char *sval;
    unsigned int inum;
    unsigned long lnum;
    unsigned long long llnum;
    void *any;
  };

  struct token_number {
    int type;
  } num;

  // True if their is whitespace between the token and the next token
  // i.e * a for operator token * would mean whitespace would be set for token
  // "a"
  bool whitespace;

  // (5+10+20)
  const char *between_brackets;
};
