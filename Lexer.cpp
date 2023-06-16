#include "Lexer.hpp"
#include "CompileProcess.hpp"
#include "CustomException.hpp"
#include "Token.hpp"
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#define LEX_GETC_IF(buffer, c, exp)                                            \
  for (c = peekChar(); exp; c = peekChar()) {                                  \
    buffer.push_back(c);                                                       \
    nextChar();                                                                \
  }

pos LexProcess::lexFilePosition() { return position; }
Token *LexProcess::tokenCreate() {
  Token *tok = new Token{};
  tok->position = lexFilePosition();

  return tok;
}

Token *LexProcess::handleWhitespace() {
  Token *lastToken = lexerLastToken();
  if (lastToken) {
    lastToken->whitespace = true;
  }
  nextChar();
  return readNextToken();
}

Token *LexProcess::lexerLastToken() {
  return (Token *)vector_back_or_null(tokenVector);
}

Token *LexProcess::createNumberToken() {
  return makeTokenNumberForValue(readNumber());
}

Token *LexProcess::makeTokenNumberForValue(unsigned long number) {
  Token *tok = tokenCreate();
  tok->type = TOKEN_TYPE_NUMBER;
  tok->memberUsed = Token::UnionMemberUsed::LLNUM;
  tok->llnum = number;
  return tok;
}

LexProcess::LexProcess(CompileProcess *process, void *privateData)
    : compileProcess(process), privateData(privateData) {
  tokenVector = vector_create(sizeof(Token));
  position.col = 1;
  position.line = 1;
  parenthesesBuffer = nullptr;
}
unsigned long long LexProcess::readNumber() {
  std::string numberStr = readNumberStr();
  return atoll(numberStr.c_str());
}
std::string LexProcess::readNumberStr() {
  std::string numString;
  char c = peekChar();
  LEX_GETC_IF(numString, c, (c >= '0' && c <= '9'));
  return numString;
}
void *LexProcess::getPrivateData() const { return this->privateData; }
vector *LexProcess::getTokenVector() const { return this->tokenVector; }
Token *LexProcess::readNextToken() {
  Token *token = nullptr;
  char c = peekChar();
  switch (c) {
  NUMERIC_CASE:
    token = createNumberToken();
    break;
  case ' ':
  case '\t':
    token = handleWhitespace();
    break;
  case '\n':
    std::cout << "we reached the end \n";
    break;
  default:
    throw CustomException(
        CompileProcess::compilerError(compileProcess, "Something went wrong"));
  }
  return token;
}
LexProcess::~LexProcess() {
  std::cout << "lex destructor \n";
  vector_free(tokenVector);
  if (parenthesesBuffer) {
    delete parenthesesBuffer;
  }
}

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
