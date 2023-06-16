#pragma once

#include "CompileProcess.hpp"
#include "Position.h"
#include "Token.hpp"
#include <string>
extern "C" {
#include "vector.h"
}

class LexProcess {

  vector *tokenVector;
  void *privateData;

private:
  Token *handleWhitespace();
  Token *lexerLastToken();

public:
  pos position;
  Token *readNextToken();
  Token *createNumberToken();
  Token *makeTokenNumberForValue(unsigned long number);
  std::string readNumberStr();
  unsigned long long readNumber();
  Token *tokenCreate();
  pos lexFilePosition();
  CompileProcess *compileProcess;
  std::string *parenthesesBuffer;
  int currentExpressionCount;
  LexProcess(CompileProcess *process, void *privateData);
  ~LexProcess();
  char nextChar();
  char peekChar();
  void pushChar(char c);
  void *getPrivateData() const;
  vector *getTokenVector() const;
};
