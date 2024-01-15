#include "compileProcess.h"
#include "compiler.h"
#include "stdio.h"
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {

  const char *inputFile = "./test.c";

  const char *outputFile = "./test";

  const char *option = "exec";

  if (argc > 1) {
    inputFile = argv[1];
  }
  if (argc > 2) {
    inputFile = argv[2];
  }
  if (argc > 3) {
    inputFile = argv[3];
  }

  int compileFlags = COMPILE_PROCESS_EXECUTE_NASM;

  if (S_EQ(option, "object")) {
    compileFlags |= COMPILE_PROCESS_EXPORT_AS_OBJECT;
  }

  int res = compileFile(inputFile, outputFile, compileFlags);

  if (res == COMPILER_FILE_COMPILED_OK) {
    printf("Compiled successfully \n");
  } else {
    printf("Compiled with errors \n");
  }

  if (compileFlags & COMPILE_PROCESS_EXECUTE_NASM) {
    char nasmOutputFile[40];
    char nasmCmd[512];

    sprintf(nasmOutputFile, "%s.o", outputFile);

    if (compileFlags & COMPILE_PROCESS_EXPORT_AS_OBJECT) {

      sprintf(nasmCmd, "nasm -f elf32 %s -o %s", outputFile, nasmOutputFile);

    } else {
      sprintf(nasmCmd, "nasm -f elf32 %s -o %s && gcc -m32 %s -o %s",
              outputFile, nasmOutputFile, nasmOutputFile, outputFile);
    }

    printf("%s\n", nasmCmd);
    int res = system(nasmCmd);

    if (res < 0) {
      printf("there was an issue \n");
      return res;
    }
  }

  return 0;
}
