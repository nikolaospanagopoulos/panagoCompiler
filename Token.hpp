#pragma once
#include "Position.h"
#include <iostream>
#include <string>

enum {

  TOKEN_TYPE_IDENTIFIER,
  TOKEN_TYPE_KEYWORD,
  TOKEN_TYPE_SYMBOL,
  TOKEN_TYPE_NUMBER,
  TOKEN_TYPE_OPERATOR,
  TOKEN_TYPE_STRING,
  TOKEN_TYPE_COMMENT,
  TOKEN_TYPE_NEWLINE,

};

class Token {
public:
  enum class UnionMemberUsed { CVAL, SVAL, INUM, LNUM, LLNUM, ANY } memberUsed;
  int type;
  int flags;

  union {
    char cval;
    const char *sval;
    unsigned int inum;
    unsigned long lnum;
    unsigned long long llnum;
    void *any;
  };
  // there is whitespace between this and the next token
  bool whitespace;
  std::string betweenBrackets;

  pos position;
  ~Token();
};
