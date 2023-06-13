#pragma once

#include "CompileProcess.hpp"
#include "Position.h"
extern "C" {
#include "vector.h"
}
#include <string>

class LexProcess {

  pos position;
  vector *tokenVector;
  CompileProcess *compileProcess;
  int currentExpressionCount;
  std::string parenthesesBuffer;
  void *privateData;

public:
  LexProcess(CompileProcess *process, void *privateData);
  ~LexProcess();
  char nextChar();
  char peekChar();
  void pushChar(char c);
  void *getPrivateData() const;
  vector *getTokenVector() const;
};
