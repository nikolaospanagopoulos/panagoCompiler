#pragma once
#include <string>

struct pos {
  int line;
  int col;
  std::string filename;
  pos();
};
