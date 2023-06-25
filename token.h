#pragma once

#include <stdbool.h>
#include <stdio.h>

typedef struct pos {

  int line;
  int col;
  const char *filename;

} pos;

enum {
  IDENTIFIER,
  KEYWORD,
  OPERATOR,
  SYMBOL,
  NUMBER,
  STRING,
  COMMENT,
  NEWLINE,
  DEFAULT
};

typedef struct token {
  int type;
  int flags;
  pos position;
  union {
    char cval;
    char *sval;
    unsigned int inum;
    unsigned long lnum;
    unsigned long long llnum;
    void *any;
  };
  bool whitespace;
  const char *betweenBrackets;
} token;

static token *handleWhitespace();
