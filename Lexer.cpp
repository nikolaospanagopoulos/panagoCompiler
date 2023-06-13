#include "Lexer.hpp"
#include "CompileProcess.hpp"
#include "Token.hpp"
#include <cstdio>

LexProcess::LexProcess(CompileProcess *process, void *privateData)
    : compileProcess(process), privateData(privateData) {
  tokenVector = vector_create(sizeof(Token));
  position.col = 1;
  position.line = 1;
}

void *LexProcess::getPrivateData() const { return this->privateData; }
vector *LexProcess::getTokenVector() const { return this->tokenVector; }

LexProcess::~LexProcess() { vector_free(tokenVector); }

char LexProcess::nextChar() {
  CompileProcess *process = compileProcess;
  process->position.col += 1;
  char c = getc(process->cfile.inputFilePtr);
  if (c == '\n') {
    process->position.line += 1;
    process->position.col = 1;
  }
  return c;
}

char LexProcess::peekChar() {
  CompileProcess *process = compileProcess;
  char c = getc(process->cfile.inputFilePtr);
  ungetc(c, process->cfile.inputFilePtr);
  return c;
}

void LexProcess::pushChar(char c) {

  CompileProcess *process = compileProcess;
  ungetc(c, process->cfile.inputFilePtr);
}
