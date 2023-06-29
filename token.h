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
enum { NORMAL, LONG, FLOAT, DOUBLE };
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
  struct tokenNumber {
    int type;
  } num;
  bool whitespace;
  const char *betweenBrackets;
} token;

static token *handleWhitespace();
