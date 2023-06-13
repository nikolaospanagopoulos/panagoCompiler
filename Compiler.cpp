#include "Compiler.hpp"
#include "CompileProcess.hpp"
#include "CustomException.hpp"
#include "Lexer.hpp"
#include <cstdio>
struct CompileProcess *Compiler::compileProcessCreate(const char *filename,
                                                      const char *filenameOut,
                                                      int flags) {

  // we use FILE because its just a ptr and we can assign in

  FILE *inFile = fopen(filename, "r");
  if (!inFile) {
    return nullptr;
  }

  // do something for error

  FILE *outFile{nullptr};
  if (filenameOut) {
    outFile = fopen(filenameOut, "w");

    if (!outFile) {
      return nullptr;
    }
  }
  CompileProcess *compileProcess = new CompileProcess{};
  compileProcess->flags = flags;
  compileProcess->cfile.inputFilePtr = inFile;
  compileProcess->outFile = outFile;
  return compileProcess;
}
int Compiler::lex() { return LEXICAL_ANALYSIS_ALL_OK; }
int Compiler::compileFile(const char *filename, const char *outFilename,
                          int flags) {
  try {
    compileProcess = compileProcessCreate(filename, outFilename, flags);

    if (!compileProcess) {
      throw CustomException("Couldnt compile file");
    }

    lexProcess = new LexProcess(compileProcess, nullptr);

    if (!lexProcess) {
      throw CustomException("lex process couldnt be created");
    }

    if (lex() != LEXICAL_ANALYSIS_ALL_OK) {
      throw CustomException("Something went wrong with the lexical analysis");
    }

  } catch (CustomException &e) {
    if (compileProcess) {
      delete compileProcess;
    }
    if (lexProcess) {
      delete lexProcess;
    }
    throw CustomException(e.what());
  }

  return 0;
}
Compiler::Compiler() : compileProcess(nullptr), lexProcess(nullptr) {}
Compiler::~Compiler() {
  if (compileProcess) {
    delete compileProcess;
  }
  if (lexProcess) {
    delete lexProcess;
  }
}
