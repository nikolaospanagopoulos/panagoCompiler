#include "Token.hpp"

Token::~Token() {
  if (sval) {
    delete[] sval;
  }
}
