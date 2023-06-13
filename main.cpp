#include "CustomException.hpp"
#include <stdexcept>
extern "C" {
#include "vector.h"
}
#include "Compiler.hpp"
#include <iostream>

int main() {

  try {
    Compiler compiler;

    compiler.compileFile("./test.c", "./test1", 0);

  } catch (CustomException &e) {
    std::cerr << e.what() << std::endl;
  } catch (const std::runtime_error &e) {
    std::cerr << e.what() << std::endl;
  } catch (...) {
    std::cerr << "Unknown failure occurred. Possible memory corruption"
              << std::endl;
  }

  return 0;
}
