#include "compileProcess.h"
#include "compiler.h"
#include "stdio.h"

int main() {

  int res = compileFile("test.c", "test", 0);

  if (res == COMPILER_FILE_COMPILED_OK) {
    printf("Compiled successfully \n");
  } else {
    printf("Compiled with errors \n");
  }
  return 0;
}
