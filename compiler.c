#include "compiler.h"
#include <stdarg.h>
#include <stdlib.h>
static struct lex_process *lexPrc;

static void setLexProcessForCompileProcess(struct lex_process *lexProcess) {
  lexPrc = lexProcess;
}

struct lex_process_functions compiler_lex_functions = {
    .next_char = compile_process_next_char,
    .peek_char = compile_process_peek_char,
    .push_char = compile_process_push_char};

void compiler_error(struct compile_process *compiler, const char *msg, ...) {
  va_list args;
  va_start(args, msg);
  vfprintf(stderr, msg, args);
  va_end(args);
  fprintf(stderr, " on line %i, col %i in file %s\n", compiler->pos.line,
          compiler->pos.col, compiler->pos.filename);
  freeLexProcess(lexPrc);
  free_compile_process(compiler);
  exit(-1);
}

void compiler_warning(struct compile_process *compiler, const char *msg, ...) {
  va_list args;
  va_start(args, msg);
  vfprintf(stderr, msg, args);
  va_end(args);
  fprintf(stderr, " on line %i, col %i in file %s\n", compiler->pos.line,
          compiler->pos.col, compiler->pos.filename);
}

int compile_file(const char *filename, const char *out_filename, int flags) {
  struct compile_process *process =
      compile_process_create(filename, out_filename, flags);
  if (!process)
    return COMPILER_FAILED_WITH_ERRORS;

  // Preform lexical analysis
  struct lex_process *lex_process =
      lex_process_create(process, &compiler_lex_functions, NULL);

  setLexProcessForCompileProcess(lex_process);

  if (!lex_process) {
    return COMPILER_FAILED_WITH_ERRORS;
  }

  if (lex(lex_process) != LEXICAL_ANALYSIS_ALL_OK) {
    return COMPILER_FAILED_WITH_ERRORS;
  }

  process->token_vec = lex_process->token_vec;

  // Preform parsing
  set_compile_process_for_scope(process);
  set_compile_process_for_array(process);
  set_compile_process_for_helpers(process);

  if (parse(process) != PARSE_ALL_OK) {
    freeLexProcess(lex_process);
    return COMPILER_FAILED_WITH_ERRORS;
  }

  set_compile_process_for_stack_frame(process);
  set_compile_process_for_resolver_default_handler(process);

  if (codegen(process) != CODEGEN_ALL_OK) {
    freeLexProcess(lex_process);
    return COMPILER_FAILED_WITH_ERRORS;
  }

  // Preform code generation..

  freeLexProcess(lex_process);
  free_compile_process(process);
  return COMPILER_FILE_COMPILED_OK;
}
