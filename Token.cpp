#include "Token.hpp"
#include <cstring>
Token::~Token() {
  if (memberUsed == UnionMemberUsed::SVAL) {
    delete[] sval;
  }
}
