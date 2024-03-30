#include "compiler.h"
#include "vector.h"
#include <assert.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define STRUCTURE_PUSH_START_POSITION_ONE 1

static struct compile_process *current_process = NULL;
static struct node *current_function = NULL;

enum {
  RESPONSE_FLAG_ACKNOWLEDGED = 0b00000001,
  RESPONSE_FLAG_PUSHED_STRUCTURE = 0b00000010,
  RESPONSE_FLAG_RESOLVED_ENTITY = 0b00000100,
  RESPONSE_FLAG_UNARY_GET_ADDRESS = 0b00001000,
};
enum {
  CODEGEN_ENTITY_RULE_IS_STRUCT_OR_UNION_NON_POINTER = 0b00000001,
  CODEGEN_ENTITY_RULE_IS_FUNCTION_CALL = 0b00000010,
  CODEGEN_ENTITY_RULE_IS_GET_ADDRESS = 0b00000100,
  CODEGEN_ENTITY_RULE_WILL_PEEK_AT_EBX = 0b00001000,
};

#define RESPONSE_SET(x) (&(struct response){x})
#define RESPONSE_EMPTY_RESPONSE_SET()

struct response_data {
  union {
    struct resolver_entity *resolved_entity;
  };
};

struct response {
  int flags;
  struct response_data data;
};

void codegen_response_expect() {
  struct response *res = calloc(1, sizeof(struct response));
  vector_push(current_process->generator->responses, &res);
}

struct response_data *codegen_response_data(struct response *response) {
  return &response->data;
}

struct response *codegen_response_pull() {
  struct response *res =
      vector_back_ptr_or_null(current_process->generator->responses);
  if (res) {
    vector_pop(current_process->generator->responses);
  }
  return res;
}

void codegen_response_acknowledge(struct response *response_in) {
  struct response *res =
      vector_back_ptr_or_null(current_process->generator->responses);
  if (res) {
    res->flags |= response_in->flags;
    if (response_in->data.resolved_entity) {
      res->data.resolved_entity = response_in->data.resolved_entity;
    }
    res->flags |= RESPONSE_FLAG_ACKNOWLEDGED;
  }
}

bool codegen_response_acknowledged(struct response *res) {
  return res && res->flags && RESPONSE_FLAG_ACKNOWLEDGED;
}

bool codegen_response_has_entity(struct response *res) {
  return codegen_response_acknowledged(res) &&
         res->flags & RESPONSE_FLAG_RESOLVED_ENTITY &&
         res->data.resolved_entity;
}

struct history_exp {
  const char *logical_start_op;
  char logical_end_label[20];
  char logical_end_label_positive[20];
};

struct history {
  int flags;
  union {
    struct history_exp exp;
  };
};

static struct history *history_begin(int flags) {
  struct history *history = calloc(1, sizeof(struct history));
  history->flags = flags;
  vector_push(current_process->gb, &history);
  return history;
}

static struct history *history_down(struct history *history, int flags) {
  struct history *new_history = calloc(1, sizeof(struct history));
  memcpy(new_history, history, sizeof(struct history));
  new_history->flags = flags;
  vector_push(current_process->gb, &new_history);
  return new_history;
}

void codegen_generate_entity_access_for_unary_get_address(
    struct resolver_result *result, struct resolver_entity *entity);
bool asm_datatype_back(struct datatype *dtype_out);
void codegen_generate_structure_push(struct resolver_entity *entity,
                                     struct history *history, int startPos);
void codegen_generate_exp_node(struct node *node, struct history *history);
const char *codegen_sub_register(const char *original_register, size_t size);
void codegen_generate_entity_access_for_function_call(
    struct resolver_result *result, struct resolver_entity *entity);
void codegen_generate_expressionable(struct node *node,
                                     struct history *history);
bool codegen_resolve_node_for_value(struct node *node, struct history *history);

void asm_pop_ebp_no_stack_frame_restore();

void codegen_generate_statement_return(struct node *node);

void _codegen_generate_if_stmt(struct node *node, int end_label_id);
void codegen_generate_body(struct node *node, struct history *history);

void codegen_new_scope(int flags) {
  resolver_default_new_scope(current_process->resolver, flags);
}
int codegen_label_count();

void codegen_finish_scope() {
  resolver_default_finish_scope(current_process->resolver);
}

struct node *codegen_node_next() {
  return vector_peek_ptr(current_process->node_tree_vec);
}

struct resolver_default_entity_data *
codegen_entity_private(struct resolver_entity *entity) {
  return resolver_default_entity_private(entity);
}

void asm_push_args(const char *ins, va_list args) {
  va_list args2;
  va_copy(args2, args);
  vfprintf(stdout, ins, args);
  fprintf(stdout, "\n");
  if (current_process->ofile) {
    vfprintf(current_process->ofile, ins, args2);
    fprintf(current_process->ofile, "\n");
  }
}

void asm_push(const char *ins, ...) {
  va_list args;
  va_start(args, ins);
  asm_push_args(ins, args);
  va_end(args);
}

void asm_push_no_nl(const char *ins, ...) {
  va_list args;
  va_start(args, ins);
  vfprintf(stdout, ins, args);
  va_end(args);

  if (current_process->ofile) {
    va_list args;
    va_start(args, ins);
    vfprintf(current_process->ofile, ins, args);
    va_end(args);
  }
}

void asm_push_ins_push(const char *fmt, int stack_entity_type,
                       const char *stack_entity_name, ...) {
  char tmp_buf[200];
  sprintf(tmp_buf, "push %s", fmt);
  va_list args;
  va_start(args, stack_entity_name);
  asm_push_args(tmp_buf, args);
  va_end(args);

  if (!current_function) {
    compiler_error(current_process,
                   "codegen error: there is no current function \n");
  }
  stackframe_push(current_function,
                  &(struct stack_frame_element){.type = stack_entity_type,
                                                .name = stack_entity_name});
}

int asm_push_ins_pop(const char *fmt, int expecting_stack_entity_type,
                     const char *expecting_stack_entity_name, ...) {
  char tmp_buf[200];
  sprintf(tmp_buf, "pop %s", fmt);
  va_list args;
  va_start(args, expecting_stack_entity_name);
  asm_push_args(tmp_buf, args);
  va_end(args);

  if (!current_function) {
    compiler_error(current_process,
                   "codegen error: there is no current function \n");
  }
  struct stack_frame_element *element = stackframe_back(current_function);
  int flags = element->flags;
  stackframe_pop_expecting(current_function, expecting_stack_entity_type,
                           expecting_stack_entity_name);
  return flags;
}

void asm_push_ins_push_with_data(const char *fmt, int stack_entity_type,
                                 const char *stack_entity_name, int flags,
                                 struct stack_frame_data *data, ...) {
  char tmp_buf[200];
  sprintf(tmp_buf, "push %s", fmt);
  va_list args;
  va_start(args, data);
  asm_push_args(tmp_buf, args);
  va_end(args);

  flags |= STACK_FRAME_ELEMENT_FLAG_HAS_DATATYPE;
  if (!current_function) {
    compiler_error(current_process,
                   "codegen error: there is no current function \n");
  }

  stackframe_push(current_function,
                  &(struct stack_frame_element){.type = stack_entity_type,
                                                .name = stack_entity_name,
                                                .flags = flags,
                                                .data = *data});
}
void asm_push_ebp() {
  asm_push_ins_push("ebp", STACK_FRAME_ELEMENT_TYPE_SAVED_BP,
                    "function_entry_saved_ebp");
}

void asm_pop_ebp() {
  asm_push_ins_pop("ebp", STACK_FRAME_ELEMENT_TYPE_SAVED_BP,
                   "function_entry_saved_ebp");
}

void codegen_stack_sub_with_name(size_t stack_size, const char *name) {
  if (stack_size != 0) {
    stackframe_sub(current_function, STACK_FRAME_ELEMENT_TYPE_UNKNOWN, name,
                   stack_size);
    asm_push("sub esp, %lld", stack_size);
  }
}
void codegen_stack_sub(size_t stack_size) {
  codegen_stack_sub_with_name(stack_size, "literal_stack_change");
}

void codegen_stack_add_with_name(size_t stack_size, const char *name) {
  if (stack_size != 0) {
    stackframe_add(current_function, STACK_FRAME_ELEMENT_TYPE_UNKNOWN, name,
                   stack_size);
    asm_push("add esp, %lld", stack_size);
  }
}

void codegen_stack_add(size_t stack_size) {
  codegen_stack_add_with_name(stack_size, "literal_stack_change");
}

struct resolver_entity *codegen_new_scope_entity(struct node *var_node,
                                                 int offset, int flags) {
  return resolver_default_new_scope_entity(current_process->resolver, var_node,
                                           offset, flags);
}

const char *codegen_get_label_for_string(const char *str) {
  const char *result = NULL;
  struct code_generator *generator = current_process->generator;
  vector_set_peek_pointer(generator->string_table, 0);
  struct string_table_element *current =
      vector_peek_ptr(generator->string_table);
  while (current) {
    if (S_EQ(current->str, str)) {
      result = current->label;
      break;
    }

    current = vector_peek_ptr(generator->string_table);
  }

  return result;
}

const char *codegen_register_string(const char *str) {
  const char *label = codegen_get_label_for_string(str);
  if (label) {
    // We already registered this string, just return the label to the string
    // memory.
    return label;
  }

  struct string_table_element *str_elem =
      calloc(1, sizeof(struct string_table_element));
  int label_id = codegen_label_count();
  sprintf((char *)str_elem->label, "str_%i", label_id);
  str_elem->str = str;
  vector_push(current_process->generator->string_table, &str_elem);
  return str_elem->label;
}

struct code_generator *codegenerator_new(struct compile_process *process) {
  struct code_generator *generator = calloc(1, sizeof(struct code_generator));
  generator->string_table =
      vector_create(sizeof(struct string_table_element *));
  generator->entry_points = vector_create(sizeof(struct codegen_entry_point *));
  generator->exit_points = vector_create(sizeof(struct codegen_exit_point *));
  generator->responses = vector_create(sizeof(struct response *));
  return generator;
}

void codegen_register_exit_point(int exit_point_id) {
  struct code_generator *gen = current_process->generator;
  struct codegen_exit_point *exit_point =
      calloc(1, sizeof(struct codegen_exit_point));
  exit_point->id = exit_point_id;
  vector_push(gen->exit_points, &exit_point);
}

struct codegen_exit_point *codegen_current_exit_point() {
  struct code_generator *gen = current_process->generator;
  return vector_back_ptr_or_null(gen->exit_points);
}

int codegen_label_count() {
  static int count = 0;
  count++;
  return count;
}
void codegen_begin_exit_point() {
  int exit_point_id = codegen_label_count();
  codegen_register_exit_point(exit_point_id);
}

void codegen_end_exit_point() {
  struct code_generator *gen = current_process->generator;
  struct codegen_exit_point *exit_point = codegen_current_exit_point();
  if (!exit_point) {
    compiler_error(current_process, "codegen error: not an exit point\n");
  }
  asm_push(".exit_point_%i:", exit_point->id);
  free(exit_point);
  vector_pop(gen->exit_points);
}

void codegen_goto_exit_point(struct node *node) {
  struct code_generator *gen = current_process->generator;
  struct codegen_exit_point *exit_point = codegen_current_exit_point();
  asm_push("jmp .exit_point_%i", exit_point->id);
}

void codegen_register_entry_point(int entry_point_id) {
  struct code_generator *gen = current_process->generator;
  struct codegen_entry_point *entry_point =
      calloc(1, sizeof(struct codegen_entry_point));
  entry_point->id = entry_point_id;
  vector_push(gen->entry_points, &entry_point);
}

struct codegen_entry_point *codegen_current_entry_point() {
  struct code_generator *gen = current_process->generator;
  return vector_back_ptr_or_null(gen->entry_points);
}

void codegen_begin_entry_point() {
  int entry_point_id = codegen_label_count();
  codegen_register_entry_point(entry_point_id);
  asm_push(".entry_point_%i:", entry_point_id);
}

void codegen_end_entry_point() {
  struct code_generator *gen = current_process->generator;
  struct codegen_entry_point *entry_point = codegen_current_entry_point();
  if (!entry_point) {
    compiler_error(current_process, "codegen error: not an entry point\n");
  }
  free(entry_point);
  vector_pop(gen->entry_points);
}

void codegen_goto_entry_point(struct node *current_node) {
  struct code_generator *gen = current_process->generator;
  struct codegen_entry_point *entry_point = codegen_current_entry_point();
  asm_push("jmp .entry_point_%i", entry_point->id);
}

void codegen_begin_entry_exit_point() {
  codegen_begin_entry_point();
  codegen_begin_exit_point();
}

void codegen_end_entry_exit_point() {
  codegen_end_entry_point();
  codegen_end_exit_point();
}

static const char *asm_keyword_for_size(size_t size, char *tmp_buf) {
  const char *keyword = NULL;
  switch (size) {
  case DATA_SIZE_BYTE:
    keyword = "db";
    break;
  case DATA_SIZE_WORD:
    keyword = "dw";
    break;
  case DATA_SIZE_DWORD:
    keyword = "dd";
    break;

  case DATA_SIZE_DDWORD:
    keyword = "dq";
    break;

  default:
    sprintf(tmp_buf, "times %lu db ", (unsigned long)size);
    return tmp_buf;
  }

  strcpy(tmp_buf, keyword);
  return tmp_buf;
}

void codegen_generate_global_variable_for_primitive(struct node *node) {
  char tmp_buf[256];
  if (node->var.val != NULL) {
    // Handle the value.
    if (node->var.val->type == NODE_TYPE_STRING) {
      const char *label = codegen_register_string(node->var.val->sval);
      asm_push("%s: %s %s", node->var.name,
               asm_keyword_for_size(variable_size(node), tmp_buf), label);
    } else {
      asm_push("%s: %s %lld", node->var.name,
               asm_keyword_for_size(variable_size(node), tmp_buf),
               node->var.val->llnum);
    }
  } else {
    asm_push("%s: %s 0", node->var.name,
             asm_keyword_for_size(variable_size(node), tmp_buf));
  }
}
void codegen_generate_global_variable_for_struct(struct node *node) {
  if (node->var.val != NULL) {
    compiler_error(current_process,
                   "we dont support values for structures yet \n");
  }
  char tmp_buf[256];
  asm_push("%s: %s 0", node->var.name,
           asm_keyword_for_size(variable_size(node), tmp_buf));
}
void codegen_generate_global_variable(struct node *node) {
  asm_push("; %s %s", node->var.type.type_str, node->var.name);
  switch (node->var.type.type) {
  case DATA_TYPE_VOID:
  case DATA_TYPE_CHAR:
  case DATA_TYPE_SHORT:
  case DATA_TYPE_INTEGER:
  case DATA_TYPE_LONG:
    codegen_generate_global_variable_for_primitive(node);
    break;
  case DATA_TYPE_STRUCT:
    codegen_generate_global_variable_for_struct(node);
    break;
  case DATA_TYPE_DOUBLE:
  case DATA_TYPE_FLOAT:
    compiler_error(current_process,
                   "Doubles and floats are not supported in our subset of C\n");
    break;
  }
}

void codegen_generate_struct(struct node *node) {
  if (node->flags & NODE_FLAG_HAS_VARIABLE_COMBINED) {
    codegen_generate_global_variable(node->_struct.var);
  }
}

void codegen_generate_data_section_part(struct node *node) {
  switch (node->type) {
  case NODE_TYPE_VARIABLE:
    codegen_generate_global_variable(node);
    break;
  case NODE_TYPE_STRUCT:
    codegen_generate_struct(node);
    break;

  default:
    break;
  }
}
void codegen_generate_data_section() {
  asm_push("section .data");
  struct node *node = codegen_node_next();
  while (node) {
    codegen_generate_data_section_part(node);
    node = codegen_node_next();
  }
}

struct resolver_entity *codegen_register_function(struct node *func_node,
                                                  int flags) {
  return resolver_default_register_function(current_process->resolver,
                                            func_node, flags);
}
void codegen_generate_function_prototype(struct node *node) {
  codegen_register_function(node, 0);
  asm_push("extern %s", node->func.name);
}

void codegen_generate_function_arguments(struct vector *argument_vector) {
  vector_set_peek_pointer(argument_vector, 0);
  struct node *current = vector_peek_ptr(argument_vector);
  while (current) {
    codegen_new_scope_entity(current, current->var.aoffset,
                             RESOLVER_DEFAULT_ENTITY_FLAG_IS_LOCAL_STACK);
    current = vector_peek_ptr(argument_vector);
  }
}

void codegen_generate_number_node(struct node *node, struct history *history) {
  asm_push_ins_push_with_data(
      "dword %i", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value",
      STACK_FRAME_ELEMENT_FLAG_IS_NUMERICAL,
      &(struct stack_frame_data){.dtype = datatype_for_numeric()}, node->llnum);
}

bool codegen_is_exp_root_for_flags(int flags) {
  return !(flags & EXPRESSION_IS_NOT_ROOT_NODE);
}

bool codegen_is_exp_root(struct history *history) {
  return codegen_is_exp_root_for_flags(history->flags);
}

void codegen_reduce_register(const char *reg, size_t size, bool is_signed) {
  if (size != DATA_SIZE_DWORD) {
    const char *ins = "movsx";
    if (!is_signed) {
      ins = "movzx";
    }
    asm_push("%s eax, %s", codegen_sub_register("eax", size));
  }
}
void asm_push_ins_push_with_flags(const char *fmt, int stack_entity_type,
                                  const char *stack_entity_name, int flags,
                                  ...) {

  char tmp_buf[200];
  sprintf(tmp_buf, "push %s", fmt);
  va_list args;
  va_start(args, flags);
  asm_push_args(tmp_buf, args);
  va_end(args);
  if (!current_function) {
    compiler_error(current_process, "Asm push error: no current function \n");
  }
  stackframe_push(current_function,
                  &(struct stack_frame_element){.flags = flags,
                                                .type = stack_entity_type,
                                                .name = stack_entity_name});
}
void codegen_gen_mem_access_get_address(struct node *node, int flags,
                                        struct resolver_entity *entity) {
  asm_push("lea ebx, [%s]", codegen_entity_private(entity)->address);
  asm_push_ins_push_with_flags("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE,
                               "result_value",
                               STACK_FRAME_ELEMENT_FLAG_IS_PUSHED_ADDRESS);
}
void codegen_generate_structure_push_or_return(struct resolver_entity *entity,
                                               struct history *history,
                                               int start_pos) {
  codegen_generate_structure_push(entity, history, start_pos);
}
void codegen_gen_mem_access(struct node *node, int flags,
                            struct resolver_entity *entity) {

  if (flags & EXPRESSION_GET_ADDRESS) {
    codegen_gen_mem_access_get_address(node, flags, entity);
    return;
  }

  if (datatype_is_struct_or_union_non_pointer(&entity->dtype)) {
    codegen_gen_mem_access_get_address(node, 0, entity);
    asm_push_ins_pop("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE,
                     "result_value");
    codegen_generate_structure_push_or_return(entity, history_begin(0), 0);
  }

  else if (datatype_element_size(&entity->dtype) != DATA_SIZE_DWORD) {
    asm_push("mov eax, [%s]", codegen_entity_private(entity)->address);
    codegen_reduce_register("eax", datatype_element_size(&entity->dtype),
                            entity->dtype.flags & DATATYPE_FLAG_IS_SIGNED);
    asm_push_ins_push_with_data(
        "eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
        &(struct stack_frame_data){.dtype = entity->dtype});
  } else {
    // We can push this straight to the stack
    asm_push_ins_push_with_data(
        "dword [%s]", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
        &(struct stack_frame_data){.dtype = entity->dtype},
        codegen_entity_private(entity)->address);
  }
}
void codegen_generate_variable_access_for_entity(struct node *node,
                                                 struct resolver_entity *entity,
                                                 struct history *history) {
  codegen_gen_mem_access(node, history->flags, entity);
}
void codegen_generate_variable_access(struct node *node,
                                      struct resolver_entity *entity,
                                      struct history *history) {
  codegen_generate_variable_access_for_entity(
      node, entity, history_down(history, history->flags));
}
void codegen_generate_identifier(struct node *node, struct history *history) {
  struct resolver_result *result =
      resolver_follow(current_process->resolver, node);
  if (!resolver_result_ok(result)) {
    compiler_error(
        current_process,
        "codegen error: something went wrong with the resolver result \n");
  }

  struct resolver_entity *entity = resolver_result_entity(result);
  codegen_generate_variable_access(node, entity, history);
  codegen_response_acknowledge(&(struct response){
      .flags = RESPONSE_FLAG_RESOLVED_ENTITY, .data.resolved_entity = entity});
}
void codegen_generate_unary_address(struct node *node,
                                    struct history *history) {
  int flags;
  codegen_generate_expressionable(
      node->unary.operand,
      history_down(history, flags | EXPRESSION_GET_ADDRESS));
  codegen_response_acknowledge(
      &(struct response){.flags = RESPONSE_FLAG_UNARY_GET_ADDRESS});
}

void codegen_generate_unary_indirection(struct node *node,
                                        struct history *history) {
  const char *regToUse = "ebx";
  int flags = history->flags;
  codegen_response_expect();
  codegen_generate_expressionable(
      node->unary.operand,
      history_down(history,
                   flags | EXPRESSION_GET_ADDRESS | EXPRESSION_INDIRECTION));

  struct response *res = codegen_response_pull();
  if (!codegen_response_has_entity(res)) {
    compiler_error(current_process, "codegen error: no response entity\n");
  }
  struct datatype operandDtype;
  if (!asm_datatype_back(&operandDtype)) {
    compiler_error(current_process, "codegen error: last operand doesnt exist "
                                    "or doesnt have a data type \n");
  }
  asm_push_ins_pop(regToUse, STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE,
                   "result_value");
  int depth = node->unary.indirection.depth;
  int real_depth = depth;
  if (!(history->flags & EXPRESSION_GET_ADDRESS)) {
    depth++;
  }
  for (int i = 0; i < depth; i++) {
    asm_push("mov %s, [%s]", regToUse, regToUse);
  }
  if (real_depth == res->data.resolved_entity->dtype.pointer_depth) {
    codegen_reduce_register(regToUse, datatype_size_no_ptr(&operandDtype),
                            operandDtype.flags & DATATYPE_FLAG_IS_SIGNED);
  }
  asm_push_ins_push_with_data(
      regToUse, STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
      &(struct stack_frame_data){.dtype = operandDtype});
  codegen_response_acknowledge(
      &(struct response){.flags = RESPONSE_FLAG_RESOLVED_ENTITY,
                         .data.resolved_entity = res->data.resolved_entity});
}

void codegen_generate_normal_unary(struct node *node, struct history *history) {
  codegen_generate_expressionable(node->unary.operand, history);

  struct datatype last_dtype;
  if (!asm_datatype_back(&last_dtype)) {
    compiler_error(current_process,
                   "codegen error: last datatype doesn't exist\n");
  }
  asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE,
                   "result_value");
  if (S_EQ(node->unary.op, "-")) {
    asm_push("neg eax");
    asm_push_ins_push_with_data(
        "eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
        &(struct stack_frame_data){.dtype = last_dtype});
  } else if (S_EQ(node->unary.op, "~")) {
    asm_push_ins_push_with_data(
        "eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
        &(struct stack_frame_data){.dtype = last_dtype});
  } else if (S_EQ(node->unary.op, "*")) {
    codegen_generate_unary_indirection(node, history);
  }
}

void codegen_generate_unary(struct node *node, struct history *history) {
  int flags = history->flags;
  if (codegen_resolve_node_for_value(node, history)) {
    return;
  }
  if (op_is_indirection(node->unary.op)) {
    codegen_generate_unary_indirection(node, history);
    return;
  } else if (op_is_address(node->unary.op)) {
    codegen_generate_unary_address(node, history);
    return;
  }
  codegen_generate_normal_unary(node, history);
}
void codegen_generate_expressionable(struct node *node,
                                     struct history *history) {
  bool is_root = codegen_is_exp_root(history);
  if (is_root) {
    history->flags |= EXPRESSION_IS_NOT_ROOT_NODE;
  }

  switch (node->type) {

  case NODE_TYPE_IDENTIFIER:
    codegen_generate_identifier(node, history);
    break;
  case NODE_TYPE_NUMBER:
    codegen_generate_number_node(node, history);
    break;

  case NODE_TYPE_EXPRESSION:
    codegen_generate_exp_node(node, history);
    break;
  case NODE_TYPE_UNARY:
    codegen_generate_unary(node, history);
    break;
  }
}

const char *codegen_sub_register(const char *original_register, size_t size) {
  const char *reg = NULL;
  if (S_EQ(original_register, "eax")) {
    if (size == DATA_SIZE_BYTE) {
      reg = "al";
    } else if (size == DATA_SIZE_WORD) {
      reg = "ax";
    } else if (size == DATA_SIZE_DWORD) {
      reg = "eax";
    }
  } else if (S_EQ(original_register, "ebx")) {
    if (size == DATA_SIZE_BYTE) {
      reg = "bl";
    } else if (size == DATA_SIZE_WORD) {
      reg = "bx";
    } else if (size == DATA_SIZE_DWORD) {
      reg = "ebx";
    }
  } else if (S_EQ(original_register, "ecx")) {
    if (size == DATA_SIZE_BYTE) {
      reg = "cl";
    } else if (size == DATA_SIZE_WORD) {
      reg = "cx";
    } else if (size == DATA_SIZE_DWORD) {
      reg = "ecx";
    }
  } else if (S_EQ(original_register, "edx")) {
    if (size == DATA_SIZE_BYTE) {
      reg = "dl";
    } else if (size == DATA_SIZE_WORD) {
      reg = "dx";
    } else if (size == DATA_SIZE_DWORD) {
      reg = "edx";
    }
  }

  return reg;
}
const char *codegen_byte_word_or_dword_or_ddword(size_t size,
                                                 const char **reg_to_use) {
  const char *type = NULL;
  const char *new_register = *reg_to_use;
  if (size == DATA_SIZE_BYTE) {
    type = "byte";
    new_register = codegen_sub_register(*reg_to_use, DATA_SIZE_BYTE);
  } else if (size == DATA_SIZE_WORD) {
    type = "word";
    new_register = codegen_sub_register(*reg_to_use, DATA_SIZE_WORD);
  } else if (size == DATA_SIZE_DWORD) {
    type = "dword";
    new_register = codegen_sub_register(*reg_to_use, DATA_SIZE_DWORD);
  } else if (size == DATA_SIZE_DDWORD) {
    type = "ddword";
    new_register = codegen_sub_register(*reg_to_use, DATA_SIZE_DDWORD);
  }
  *reg_to_use = new_register;
  return type;
}

void codegen_generate_assignment_instruction_for_operator(
    const char *mov_type_keyword, const char *address, const char *reg_to_use,
    const char *op, bool is_signed) {
  if (S_EQ(op, "=")) {
    asm_push("mov %s [%s], %s", mov_type_keyword, address, reg_to_use);
  } else if (S_EQ(op, "+=")) {
    asm_push("add %s [%s], %s", mov_type_keyword, address, reg_to_use);
  }
}
void codegen_generate_scope_variable(struct node *node) {
  struct resolver_entity *entity = codegen_new_scope_entity(
      node, node->var.aoffset, RESOLVER_DEFAULT_ENTITY_FLAG_IS_LOCAL_STACK);
  if (node->var.val) {
    codegen_generate_expressionable(
        node->var.val, history_begin(EXPRESSION_IS_ASSIGNMENT |
                                     IS_RIGHT_OPERAND_OF_ASSIGNMENT));
    // pop eax
    asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE,
                     "result_value");
    const char *reg_to_use = "eax";
    const char *mov_type = codegen_byte_word_or_dword_or_ddword(
        datatype_element_size(&entity->dtype), &reg_to_use);
    codegen_generate_assignment_instruction_for_operator(
        mov_type, codegen_entity_private(entity)->address, reg_to_use, "=",
        entity->dtype.flags & DATATYPE_FLAG_IS_SIGNED);
  }
}

void codegen_generate_entity_access_start(
    struct resolver_result *result,
    struct resolver_entity *root_assignment_entity, struct history *history) {
  if (root_assignment_entity->type == RESOLVER_ENTITY_TYPE_UNSUPPORTED) {
    // Unsupported entity then process it.
    codegen_generate_expressionable(root_assignment_entity->node, history);
  } else if (result->flags & RESOLVER_RESULT_FLAG_FIRST_ENTITY_PUSH_VALUE) {
    asm_push_ins_push_with_data(
        "dword [%s]", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
        &(struct stack_frame_data){.dtype = root_assignment_entity->dtype},
        result->base.address);
  } else if (result->flags & RESOLVER_RESULT_FLAG_FIRST_ENTITY_LOAD_TO_EBX) {
    if (root_assignment_entity->next &&
        root_assignment_entity->next->flags &
            RESOLVER_ENTITY_FLAG_IS_POINTER_ARRAY_ENTITY) {
      asm_push("mov ebx, [%s]", result->base.address);
    } else {
      asm_push("lea ebx, [%s]", result->base.address);
    }
    asm_push_ins_push_with_data(
        "ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
        &(struct stack_frame_data){.dtype = root_assignment_entity->dtype});
  }
}

void codegen_generate_entity_access_for_variable_or_general(
    struct resolver_result *result, struct resolver_entity *entity) {
  // Restore the EBX register
  asm_push_ins_pop("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE,
                   "result_value");
  if (entity->flags & RESOLVER_ENTITY_FLAG_DO_INDIRECTION) {
    asm_push("mov ebx, [ebx]");
  }
  asm_push("add ebx, %i", entity->offset);
  asm_push_ins_push_with_data(
      "ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
      &(struct stack_frame_data){.dtype = entity->dtype});
}
int codegen_entity_rules(struct resolver_entity *lastEntity,
                         struct history *history) {
  int ruleFlags = 0;
  if (!lastEntity) {
    return 0;
  }
  if (datatype_is_struct_or_union_non_pointer(&lastEntity->dtype)) {
    ruleFlags |= CODEGEN_ENTITY_RULE_IS_STRUCT_OR_UNION_NON_POINTER;
  }
  if (lastEntity->type == RESOLVER_ENTITY_TYPE_FUNCTION_CALL) {
    ruleFlags |= CODEGEN_ENTITY_RULE_IS_FUNCTION_CALL;
  } else if (history->flags & EXPRESSION_GET_ADDRESS) {
    ruleFlags |= CODEGEN_ENTITY_RULE_IS_GET_ADDRESS;
  } else if (lastEntity->type == RESOLVER_ENTITY_TYPE_UNARY_GET_ADDRESS) {
    ruleFlags |= CODEGEN_ENTITY_RULE_IS_GET_ADDRESS;
  } else {
    ruleFlags |= CODEGEN_ENTITY_RULE_WILL_PEEK_AT_EBX;
  }
  return ruleFlags;
}
void codegen_apply_unary_access(int depth) {
  for (int i = 0; i < depth; i++) {
    asm_push("mov ebx, [ebx]");
  }
}
void codegen_generate_entity_access_for_unary_indirection_for_assignment_left_operand(
    struct resolver_result *result, struct resolver_entity *entity,
    struct history *history) {
  asm_push("; INDIRECTION");
  int flags = asm_push_ins_pop("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE,
                               "result_value");
  int gen_entity_rules = codegen_entity_rules(result->last_entity, history);
  int depth = entity->indirection.depth - 1;
  codegen_apply_unary_access(depth);
  asm_push_ins_push_with_flags("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE,
                               "result_value",
                               STACK_FRAME_ELEMENT_FLAG_IS_PUSHED_ADDRESS);
}
void codegen_generate_entity_access_for_unsupported(
    struct resolver_result *result, struct resolver_entity *entity) {
  codegen_generate_expressionable(entity->node, history_begin(0));
}
void codegen_generate_entity_access_for_entity_for_assignment_left_operand(
    struct resolver_result *result, struct resolver_entity *entity,
    struct history *history) {
  switch (entity->type) {
  case RESOLVER_ENTITY_TYPE_ARRAY_BRACKET:
#warning "todo implement array bracket"
    break;

  case RESOLVER_ENTITY_TYPE_VARIABLE:
  case RESOLVER_ENTITY_TYPE_GENERAL:
    codegen_generate_entity_access_for_variable_or_general(result, entity);
    break;

  case RESOLVER_ENTITY_TYPE_FUNCTION_CALL:
    codegen_generate_entity_access_for_function_call(result, entity);
    break;

  case RESOLVER_ENTITY_TYPE_UNARY_INDIRECTION:
    codegen_generate_entity_access_for_unary_indirection_for_assignment_left_operand(
        result, entity, history);
    break;

  case RESOLVER_ENTITY_TYPE_UNARY_GET_ADDRESS:
    codegen_generate_entity_access_for_unary_get_address(result, entity);
    break;

  case RESOLVER_ENTITY_TYPE_UNSUPPORTED:
    codegen_generate_entity_access_for_unsupported(result, entity);
    break;

  case RESOLVER_ENTITY_TYPE_CAST:
#warning "cast"
    break;

  default:
    compiler_error(current_process, "COMPILER BUG...");
  }
}
void codegen_generate_entity_access_for_assignment_left_operand(
    struct resolver_result *result,
    struct resolver_entity *root_assignment_entity, struct node *top_most_node,
    struct history *history) {
  codegen_generate_entity_access_start(result, root_assignment_entity, history);
  struct resolver_entity *current =
      resolver_result_entity_next(root_assignment_entity);
  while (current) {
    codegen_generate_entity_access_for_entity_for_assignment_left_operand(
        result, current, history);
    current = resolver_result_entity_next(current);
  }
}

void codegen_plus_or_minus_string_for_value(char *out, int val, size_t len);
void codegen_generate_move_struct(struct datatype *dtype, const char *base_addr,
                                  int offset) {
  size_t structure_size = align_value(datatype_size(dtype), DATA_SIZE_DWORD);
  int pops = structure_size / DATA_SIZE_DWORD;
  for (int i = 0; i < pops; i++) {
    asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE,
                     "result_value");
    char fmt[10];
    int chunk_offset = offset + (i * DATA_SIZE_DWORD);
    codegen_plus_or_minus_string_for_value(fmt, chunk_offset, sizeof(fmt));
    asm_push("mov [%s%s], eax", base_addr, fmt);
  }
}
void codegen_generate_assignment_part(struct node *node, const char *op,
                                      struct history *history) {
  struct datatype right_operand_dtype;
  struct resolver_result *result =
      resolver_follow(current_process->resolver, node);
  if (!resolver_result_ok(result)) {
    compiler_error(
        current_process,
        "resolver error: something went wrong with the resolver result\n");
  }
  struct resolver_entity *root_assignment_entity =
      resolver_result_entity_root(result);
  const char *reg_to_use = "eax";
  const char *mov_type = codegen_byte_word_or_dword_or_ddword(
      datatype_element_size(&result->last_entity->dtype), &reg_to_use);
  struct resolver_entity *next_entity =
      resolver_result_entity_next(root_assignment_entity);
  if (!next_entity) {
    if (datatype_is_struct_or_union_non_pointer(&result->last_entity->dtype)) {
      codegen_generate_move_struct(&result->last_entity->dtype,
                                   result->base.address, 0);
    } else {
      asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE,
                       "result_value");
      codegen_generate_assignment_instruction_for_operator(
          mov_type, result->base.address, reg_to_use, op,
          result->last_entity->dtype.flags & DATATYPE_FLAG_IS_SIGNED);
    }
  } else {
    codegen_generate_entity_access_for_assignment_left_operand(
        result, root_assignment_entity, node, history);
    asm_push_ins_pop("edx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE,
                     "result_value");
    asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE,
                     "result_value");
    codegen_generate_assignment_instruction_for_operator(
        mov_type, "edx", reg_to_use, op,
        result->last_entity->flags & DATATYPE_FLAG_IS_SIGNED);
  }
}
void codegen_generate_assignment_expression(struct node *node,
                                            struct history *history) {
  codegen_generate_expressionable(
      node->exp.right,
      history_down(history,
                   EXPRESSION_IS_ASSIGNMENT | IS_RIGHT_OPERAND_OF_ASSIGNMENT));
  codegen_generate_assignment_part(node->exp.left, node->exp.op, history);
}

void codegen_generate_entity_access_for_function_call(
    struct resolver_result *result, struct resolver_entity *entity) {
  vector_set_flag(entity->func_call_data.arguments, VECTOR_FLAG_PEEK_DECREMENT);
  vector_set_peek_pointer_end(entity->func_call_data.arguments);

  int res = vector_count(entity->func_call_data.arguments);
  printf("arguments: %d\n", res);
  struct node *node = vector_peek_ptr(entity->func_call_data.arguments);
  asm_push_ins_pop("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE,
                   "result_value");
  asm_push("mov ecx, ebx");
  if (datatype_is_struct_or_union_non_pointer(&entity->dtype)) {
    asm_push("; SUBTRACT ROOM FOR RETURNED STRUCTURE/UNION DATATYPE");
    codegen_stack_sub_with_name(
        align_value(datatype_size(&entity->dtype), DATA_SIZE_DWORD),
        "result_value");
    asm_push_ins_push("esp", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE,
                      "result_value");
  }

  while (node) {
    codegen_generate_expressionable(
        node, history_begin(EXPRESSION_IN_FUNCTION_CALL_ARGUMENTS));
    node = vector_peek_ptr(entity->func_call_data.arguments);
  }
  asm_push("call ecx");
  size_t stack_size = entity->func_call_data.stack_size;
  if (datatype_is_struct_or_union_non_pointer(&entity->dtype)) {
    stack_size += DATA_SIZE_DWORD;
  }
  codegen_stack_add(stack_size);
  if (datatype_is_struct_or_union_non_pointer(&entity->dtype)) {

    asm_push("mov ebx, eax");
    codegen_generate_structure_push(entity, history_begin(0), 0);

  } else {
    asm_push_ins_push_with_data(
        "eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
        &(struct stack_frame_data){.dtype = entity->dtype});
  }

  struct resolver_entity *next_entity = resolver_result_entity_next(entity);
  if (next_entity && datatype_is_struct_or_union(&entity->dtype)) {
    asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE,
                     "result_value");
    asm_push("mov ebx, eax");
    asm_push_ins_push("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE,
                      "result_value");
  }
}
void codegen_generate_entity_access_for_unary_indirection(
    struct resolver_result *result, struct resolver_entity *entity,
    struct history *history) {
  asm_push("; INDIRECTION PIDARAS");
  struct datatype operandDtype;
  assert(asm_datatype_back(&operandDtype));
  int flags = asm_push_ins_pop("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE,
                               "result_value");
  int gen_entity_rules = codegen_entity_rules(result->last_entity, history);
  int depth = entity->indirection.depth;
  codegen_apply_unary_access(depth);
  asm_push_ins_push_with_data(
      "ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value",
      STACK_FRAME_ELEMENT_FLAG_IS_PUSHED_ADDRESS,
      &(struct stack_frame_data){.dtype = operandDtype});
}
void codegen_generate_entity_access_for_unary_get_address(
    struct resolver_result *result, struct resolver_entity *entity) {
  asm_push_ins_pop("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE,
                   "result_value");
  asm_push("; PUSH ADDRESS &");
  asm_push_ins_push_with_data(
      "ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
      &(struct stack_frame_data){.dtype = entity->dtype});
}
void codegen_generate_entity_access_for_entity(struct resolver_result *result,
                                               struct resolver_entity *entity,
                                               struct history *history) {
  switch (entity->type) {
  case RESOLVER_ENTITY_TYPE_ARRAY_BRACKET:
#warning "todo implement array bracket"
    break;

  case RESOLVER_ENTITY_TYPE_VARIABLE:
  case RESOLVER_ENTITY_TYPE_GENERAL:
    codegen_generate_entity_access_for_variable_or_general(result, entity);
    break;

  case RESOLVER_ENTITY_TYPE_FUNCTION_CALL:
    codegen_generate_entity_access_for_function_call(result, entity);
    break;

  case RESOLVER_ENTITY_TYPE_UNARY_INDIRECTION:
    codegen_generate_entity_access_for_unary_indirection(result, entity,
                                                         history);
    break;

  case RESOLVER_ENTITY_TYPE_UNARY_GET_ADDRESS:
    codegen_generate_entity_access_for_unary_get_address(result, entity);
    break;

  case RESOLVER_ENTITY_TYPE_UNSUPPORTED:
    codegen_generate_entity_access_for_unsupported(result, entity);
    break;

  case RESOLVER_ENTITY_TYPE_CAST:
#warning "cast"
    break;

  default:
    compiler_error(current_process, "COMPILER BUG...");
  }
}
void codegen_generate_entity_access(
    struct resolver_result *result,
    struct resolver_entity *root_assignment_entity, struct node *top_most_node,
    struct history *history) {
  codegen_generate_entity_access_start(result, root_assignment_entity, history);
  struct resolver_entity *current =
      resolver_result_entity_next(root_assignment_entity);
  while (current) {
    codegen_generate_entity_access_for_entity(result, current, history);
    current = resolver_result_entity_next(current);
  }

  struct resolver_entity *last_entity = result->last_entity;
  codegen_response_acknowledge(
      &(struct response){.flags = RESPONSE_FLAG_RESOLVED_ENTITY,
                         .data.resolved_entity = last_entity});
}

bool codegen_resolve_node_return_result(struct node *node,
                                        struct history *history,
                                        struct resolver_result **result_out) {
  struct resolver_result *result =
      resolver_follow(current_process->resolver, node);
  if (resolver_result_ok(result)) {
    struct resolver_entity *root_assignment_entity =
        resolver_result_entity_root(result);
    codegen_generate_entity_access(result, root_assignment_entity, node,
                                   history);
    if (result_out) {
      *result_out = result;
    }
    codegen_response_acknowledge(
        &(struct response){.flags = RESPONSE_FLAG_RESOLVED_ENTITY,
                           .data.resolved_entity = result->last_entity});
    return true;
  }

  return false;
}

bool codegen_resolve_node_for_value(struct node *node,
                                    struct history *history) {
  struct resolver_result *result = NULL;
  if (!codegen_resolve_node_return_result(node, history, &result)) {
    return false;
  }
  struct datatype dtype;
  if (!asm_datatype_back(&dtype)) {
    compiler_error(current_process, "codegen error: no datatype\n");
  }
  if (datatype_is_struct_or_union_non_pointer(&dtype)) {
    codegen_generate_structure_push(result->last_entity, history, 0);
  } else if (!(dtype.flags & DATATYPE_FLAG_IS_POINTER)) {
    asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE,
                     "result_value");
    if (result->flags &
        RESOLVER_RESULT_FLAG_FINAL_INDIRECTION_REQUIRED_FOR_VALUE) {
      asm_push("mov eax, [eax]");
    }
    codegen_reduce_register("eax", datatype_element_size(&dtype),
                            dtype.flags & DATATYPE_FLAG_IS_SIGNED);

    asm_push_ins_push_with_data("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE,
                                "result_value", 0,
                                &(struct stack_frame_data){.dtype = dtype});
  }
  return true;
}

int get_additional_flags(int current_flags, struct node *node) {
  if (node->type != NODE_TYPE_EXPRESSION) {
    return 0;
  }

  int additional_flags = 0;
  bool maintain_function_call_argument_flag =
      (current_flags & EXPRESSION_IN_FUNCTION_CALL_ARGUMENTS) &&
      S_EQ(node->exp.op, ",");
  if (maintain_function_call_argument_flag) {
    additional_flags |= EXPRESSION_IN_FUNCTION_CALL_ARGUMENTS;
  }
  return additional_flags;
}

int codegen_set_flag_for_operator(const char *op) {
  int flag = 0;
  if (S_EQ(op, "+")) {
    flag |= EXPRESSION_IS_ADDITION;
  } else if (S_EQ(op, "-")) {
    flag |= EXPRESSION_IS_SUBTRACTION;
  } else if (S_EQ(op, "*")) {
    flag |= EXPRESSION_IS_MULTIPLICATION;
  } else if (S_EQ(op, "/")) {
    flag |= EXPRESSION_IS_DIVISION;
  } else if (S_EQ(op, "%")) {
    flag |= EXPRESSION_IS_MODULAS;
  } else if (S_EQ(op, ">")) {
    flag |= EXPRESSION_IS_ABOVE;
  } else if (S_EQ(op, "<")) {
    flag |= EXPRESSION_IS_BELOW;
  } else if (S_EQ(op, ">=")) {
    flag |= EXPRESSION_IS_ABOVE_OR_EQUAL;
  } else if (S_EQ(op, "<=")) {
    flag |= EXPRESSION_IS_BELOW_OR_EQUAL;
  } else if (S_EQ(op, "!=")) {
    flag |= EXPRESSION_IS_NOT_EQUAL;
  } else if (S_EQ(op, "==")) {
    flag |= EXPRESSION_IS_EQUAL;
  } else if (S_EQ(op, "&&")) {
    flag |= EXPRESSION_LOGICAL_AND;
  } else if (S_EQ(op, "<<")) {
    flag |= EXPRESSION_IS_BITSHIFT_LEFT;
  } else if (S_EQ(op, ">>")) {
    flag |= EXPRESSION_IS_BITSHIFT_RIGHT;
  } else if (S_EQ(op, "&")) {
    flag |= EXPRESSION_IS_BITWISE_AND;
  } else if (S_EQ(op, "|")) {
    flag |= EXPRESSION_IS_BITWISE_OR;
  } else if (S_EQ(op, "^")) {
    flag |= EXPRESSION_IS_BITWISE_XOR;
  }
  return flag;
}

struct stack_frame_element *asm_stack_back() {
  return stackframe_back(current_function);
}

struct stack_frame_element *asm_stack_peek() {
  return stackframe_peek(current_function);
}

void asm_stack_peek_start() { stackframe_peek_start(current_function); }

bool asm_datatype_back(struct datatype *dtype_out) {
  struct stack_frame_element *last_stack_frame_element = asm_stack_back();
  if (!last_stack_frame_element) {
    return false;
  }

  if (!(last_stack_frame_element->flags &
        STACK_FRAME_ELEMENT_FLAG_HAS_DATATYPE)) {
    return false;
  }

  *dtype_out = last_stack_frame_element->data.dtype;
  return true;
}

bool codegen_can_gen_math(int flags) { return flags & EXPRESSION_GEN_MATHABLE; }

void codegen_gen_cmp(const char *value, const char *set_ins) {
  asm_push("cmp eax, %s", value);
  asm_push("%s al", set_ins);
  asm_push("movzx eax, al");
}

void codegen_gen_math_for_value(const char *reg, const char *value, int flags,
                                bool is_signed) {
  if (flags & EXPRESSION_IS_ADDITION) {
    asm_push("add %s, %s", reg, value);
  } else if (flags & EXPRESSION_IS_SUBTRACTION) {
    asm_push("sub %s, %s", reg, value);
  } else if (flags & EXPRESSION_IS_MULTIPLICATION) {
    asm_push("mov ecx, %s", value);
    if (is_signed) {
      asm_push("imul ecx");
    } else {
      asm_push("mul ecx");
    }
  } else if (flags & EXPRESSION_IS_DIVISION) {
    asm_push("mov ecx, %s", value);
    asm_push("cdq");
    if (is_signed) {
      asm_push("idiv ecx");
    } else {
      asm_push("div ecx");
    }
  } else if (flags & EXPRESSION_IS_MODULAS) {
    asm_push("mov ecx, %s", value);
    asm_push("cdq");
    if (is_signed) {
      asm_push("idiv ecx");
    } else {
      asm_push("div ecx");
    }

    asm_push("mov eax, edx");
  } else if (flags & EXPRESSION_IS_ABOVE) {
    codegen_gen_cmp(value, "setg");
  } else if (flags & EXPRESSION_IS_BELOW) {
    codegen_gen_cmp(value, "setl");
  } else if (flags & EXPRESSION_IS_EQUAL) {
    codegen_gen_cmp(value, "sete");
  } else if (flags & EXPRESSION_IS_ABOVE_OR_EQUAL) {
    codegen_gen_cmp(value, "setge");
  } else if (flags & EXPRESSION_IS_BELOW_OR_EQUAL) {
    codegen_gen_cmp(value, "setle");
  } else if (flags & EXPRESSION_IS_NOT_EQUAL) {
    codegen_gen_cmp(value, "setne");
  } else if (flags & EXPRESSION_IS_BITSHIFT_LEFT) {
    value = codegen_sub_register(value, DATA_SIZE_BYTE);
    asm_push("sal %s, %s", reg, value);
  } else if (flags & EXPRESSION_IS_BITSHIFT_RIGHT) {
    value = codegen_sub_register(value, DATA_SIZE_BYTE);
    asm_push("sar %s, %s", reg, value);
  } else if (flags & EXPRESSION_IS_BITWISE_AND) {
    asm_push("and %s, %s", reg, value);
  } else if (flags & EXPRESSION_IS_BITWISE_OR) {
    asm_push("or %s, %s", reg, value);
  } else if (flags & EXPRESSION_IS_BITWISE_XOR) {
    asm_push("xor %s, %s", reg, value);
  }
}

void codegen_setup_new_logical_expression(struct history *history,
                                          struct node *node) {
  int label_index = codegen_label_count();
  sprintf(history->exp.logical_end_label, ".endc_%i", label_index);
  sprintf(history->exp.logical_end_label_positive, ".endc_%i_positive",
          label_index);
  history->exp.logical_start_op = node->exp.op;
  history->flags |= EXPRESSION_IN_LOGICAL_EXPRESSION;
}

void codegen_generate_logical_cmp_and(const char *reg, const char *fail_label) {
  asm_push("cmp %s, 0", reg);
  asm_push("je %s", fail_label);
}

void codegen_generate_logical_cmp_or(const char *reg, const char *equal_label) {
  asm_push("cmp %s, 0", reg);
  asm_push("jg %s", equal_label);
}

void codegen_generate_logical_cmp(const char *op, const char *fail_label,
                                  const char *equal_label) {
  if (S_EQ(op, "&&")) {
    codegen_generate_logical_cmp_and("eax", fail_label);
  } else if (S_EQ(op, "||")) {
    codegen_generate_logical_cmp_or("eax", equal_label);
  }
}
void codegen_generate_end_labels_for_logical_expression(
    const char *op, const char *end_label, const char *end_label_positive) {
  if (S_EQ(op, "&&")) {
    asm_push("; && END CLAUSE");
    asm_push("mov eax, 1");
    asm_push("jmp %s", end_label_positive);
    asm_push("%s:", end_label);
    asm_push("xor eax, eax");
    asm_push("%s:", end_label_positive);
  } else if (S_EQ(op, "||")) {
    asm_push("; || END CLAUSE");
    asm_push("jmp %s", end_label);
    asm_push("%s:", end_label_positive);
    asm_push("mov eax, 1");
    asm_push("%s:", end_label);
  }
}
void codegen_generate_exp_node_for_logical_arithmetic(struct node *node,
                                                      struct history *history) {
  bool start_of_logical_exp =
      !(history->flags & EXPRESSION_IN_LOGICAL_EXPRESSION);
  if (start_of_logical_exp) {
    codegen_setup_new_logical_expression(history, node);
  }
  codegen_generate_expressionable(
      node->exp.left,
      history_down(history, history->flags | EXPRESSION_IN_LOGICAL_EXPRESSION));
  asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE,
                   "result_value");
  codegen_generate_logical_cmp(node->exp.op, history->exp.logical_end_label,
                               history->exp.logical_end_label_positive);
  codegen_generate_expressionable(
      node->exp.right,
      history_down(history, history->flags | EXPRESSION_IN_LOGICAL_EXPRESSION));
  if (!is_logical_node(node->exp.right)) {
    asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE,
                     "result_value");
    codegen_generate_logical_cmp(node->exp.op, history->exp.logical_end_label,
                                 history->exp.logical_end_label_positive);
    codegen_generate_end_labels_for_logical_expression(
        node->exp.op, history->exp.logical_end_label,
        history->exp.logical_end_label_positive);
    asm_push_ins_push("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE,
                      "result_value");
  }
}

struct datatype *datatype_pointer_reduce(struct datatype *dtype, int by) {
  struct datatype *newDatatype = calloc(1, sizeof(struct datatype));
  memcpy(newDatatype, dtype, sizeof(struct datatype));
  newDatatype->pointer_depth -= by;
  if (newDatatype->pointer_depth <= 0) {
    newDatatype->flags &= ~DATATYPE_FLAG_IS_POINTER;
    newDatatype->pointer_depth = 0;
  }
  vector_push(current_process->gb, &newDatatype);
  return newDatatype;
}

void codegen_generate_exp_node_for_arithmetic(struct node *node,
                                              struct history *history) {
  if (node->type != NODE_TYPE_EXPRESSION) {
    compiler_error(current_process, "codegen error: not an expression node \n");
  }
  int flags = history->flags;

  if (is_logical_operator(node->exp.op)) {
    codegen_generate_exp_node_for_logical_arithmetic(node, history);
    return;
  }

  struct node *left_node = node->exp.left;
  struct node *right_node = node->exp.right;
  int op_flags = codegen_set_flag_for_operator(node->exp.op);
  codegen_generate_expressionable(left_node, history_down(history, flags));
  codegen_generate_expressionable(right_node, history_down(history, flags));
  struct datatype last_dtype = datatype_for_numeric();
  asm_datatype_back(&last_dtype);
  if (codegen_can_gen_math(op_flags)) {
    struct datatype right_dtype = datatype_for_numeric();
    asm_datatype_back(&right_dtype);
    asm_push_ins_pop("ecx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE,
                     "result_value");
    if (last_dtype.flags & DATATYPE_FLAG_IS_LITERAL) {
      asm_datatype_back(&last_dtype);
    }

    struct datatype left_dtype = datatype_for_numeric();
    asm_datatype_back(&left_dtype);
    asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE,
                     "result_value");
    struct datatype *pointer_datatype =
        datatype_thats_a_pointer(&left_dtype, &right_dtype);
    if (pointer_datatype && datatype_size(datatype_pointer_reduce(
                                pointer_datatype, 1)) > DATA_SIZE_BYTE) {
      const char *reg = "ecx";
      if (pointer_datatype == &right_dtype) {
        reg = "eax";
      }
      asm_push("imul %s, %i", reg,
               datatype_size(datatype_pointer_reduce(pointer_datatype, 1)));
    }

    codegen_gen_math_for_value("eax", "ecx", op_flags,
                               last_dtype.flags & DATATYPE_FLAG_IS_SIGNED);
  }

  asm_push_ins_push_with_data("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE,
                              "result_value", 0,
                              &(struct stack_frame_data){.dtype = last_dtype});
}

int codegen_remove_uninheritable_flags(int flags) {
  return flags & ~EXPRESSION_UNINHERITABLE_FLAGS;
}
void codegen_generate_exp_node(struct node *node, struct history *history) {
  if (is_node_assignment(node)) {
    codegen_generate_assignment_expression(node, history);
    return;
  }

  // Can we locate a variable for a given expression?
  if (codegen_resolve_node_for_value(node, history)) {
    return;
  }

  int additional_flags = get_additional_flags(history->flags, node);
  codegen_generate_exp_node_for_arithmetic(
      node,
      history_down(history, codegen_remove_uninheritable_flags(history->flags) |
                                additional_flags));
}

void codegen_discard_unused_stack() {
  asm_stack_peek_start();

  struct stack_frame_element *element = asm_stack_peek();
  size_t stack_adjustment = 0;
  while (element) {
    if (!S_EQ(element->name, "result_value")) {
      break;
    }

    stack_adjustment += DATA_SIZE_DWORD;
    element = asm_stack_peek();
  }

  codegen_stack_add(stack_adjustment);
}

void codegen_plus_or_minus_string_for_value(char *out, int val, size_t len) {
  memset(out, 0, len);
  if (val < 0) {
    sprintf(out, "%i", val);
  } else {
    sprintf(out, "+%i", val);
  }
}

void codegen_generate_structure_push(struct resolver_entity *entity,
                                     struct history *history, int startPos) {
  asm_push("; STRUCTURE PUSH");
  size_t structure_size = align_value(entity->dtype.size, DATA_SIZE_DWORD);

  int pushes = structure_size / DATA_SIZE_DWORD;

  for (int i = pushes - 1; i >= startPos; i--) {
    char fmt[10];
    int chunk_offset = (i * DATA_SIZE_DWORD);
    codegen_plus_or_minus_string_for_value(fmt, chunk_offset, sizeof(fmt));
    asm_push_ins_push_with_data(
        "dword [%s%s]", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value",
        0, &(struct stack_frame_data){.dtype = entity->dtype}, "ebx", fmt);
  }
  asm_push("; END STRUCTURE PUSH");
  codegen_response_acknowledge(
      RESPONSE_SET(.flags = RESPONSE_FLAG_PUSHED_STRUCTURE));
}
void codegen_generate_statement_return_exp(struct node *node) {
  codegen_response_expect();
  codegen_generate_expressionable(node->stmt.return_stmt.exp,
                                  history_begin(IS_STATEMENT_RETURN));
  struct datatype dtype;
  if (!asm_datatype_back(&dtype)) {
    compiler_error(current_process, "codegen error: datatype doesnt exist\n");
  }
  if (datatype_is_struct_or_union_non_pointer(&dtype)) {
    asm_push("mov edx, [ebp+8]");
    codegen_generate_move_struct(&dtype, "edx", 0);
    asm_push("mov eax, [ebp+8]");
    return;
  }
  asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE,
                   "result_value");
}
void codegen_stack_add_no_compile_time_stack_frame_restore(size_t stackSize) {
  if (stackSize != 0) {
    asm_push("add esp, %lld", stackSize);
  }
  return;
}

void asm_pop_ebp_no_stack_frame_restore() { asm_push("pop ebp"); }

void codegen_generate_statement_return(struct node *node) {
  if (node->stmt.return_stmt.exp) {
    codegen_generate_statement_return_exp(node);
  }
  codegen_stack_add_no_compile_time_stack_frame_restore(
      C_ALIGN(function_node_stack_size(node->binded.function)));

  asm_pop_ebp_no_stack_frame_restore();
  asm_push("ret");
}
void codegen_generate_else_stmt(struct node *node) {
  codegen_generate_body(node->stmt.else_stmt.body_node, history_begin(0));
}

void codegen_generate_else_or_else_if(struct node *node, int end_label_id) {
  if (node->type == NODE_TYPE_STATEMENT_IF) {
    _codegen_generate_if_stmt(node, end_label_id);
  } else if (node->type == NODE_TYPE_STATEMENT_ELSE) {
    codegen_generate_else_stmt(node);
  } else {
    compiler_error(current_process, "Unexpected keyword\n");
  }
}

void _codegen_generate_if_stmt(struct node *node, int end_label_id) {
  int if_label_id = codegen_label_count();
  codegen_generate_expressionable(node->stmt.if_stmt.cond_node,
                                  history_begin(0));
  asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE,
                   "result_value");
  asm_push("cmp eax, 0");
  asm_push("je .if_%i", if_label_id);
  codegen_generate_body(node->stmt.if_stmt.body_node,
                        history_begin(IS_ALONE_STATEMENT));
  asm_push("jmp .if_end_%i", end_label_id);
  asm_push(".if_%i:", if_label_id);
  if (node->stmt.if_stmt.next) {
    codegen_generate_else_or_else_if(node, end_label_id);
  }
}

void codegen_generate_if_stmt(struct node *node) {
  int end_label_id = codegen_label_count();
  _codegen_generate_if_stmt(node, end_label_id);
  asm_push(".if_end_%i:", end_label_id);
}

void codegen_generate_while_stmt(struct node *node) {
  codegen_begin_entry_exit_point();
  int while_start_id = codegen_label_count();
  int while_end_id = codegen_label_count();
  asm_push(".while_start_%i:", while_start_id);
  codegen_generate_expressionable(node->stmt.while_stmt.exp_node,
                                  history_begin(0));
  asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE,
                   "result_value");
  asm_push("cmp eax, 0");
  asm_push("je .while_end_%i", while_end_id);
  codegen_generate_body(node->stmt.while_stmt.body_node,
                        history_begin(IS_ALONE_STATEMENT));
  asm_push("jmp .while_start_%i", while_start_id);
  asm_push(".while_end_%i:", while_end_id);
  codegen_end_entry_exit_point();
}

void codegen_generate_do_while_stmt(struct node *node) {
  codegen_begin_entry_exit_point();
  int doWhileStart = codegen_label_count();
  asm_push(".do_while_start_%i:", doWhileStart);
  codegen_generate_body(node->stmt.do_while_stmt.body_node,
                        history_begin(IS_ALONE_STATEMENT));
  codegen_generate_expressionable(node->stmt.do_while_stmt.exp_node,
                                  history_begin(0));
  asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE,
                   "result_value");
  asm_push("cmp eax, 0");
  asm_push("jne .do_while_start_%i", doWhileStart);
  codegen_end_entry_exit_point();
}

void codegen_generate_statement(struct node *node, struct history *history) {
  switch (node->type) {
  case NODE_TYPE_EXPRESSION:
    codegen_generate_exp_node(node, history_begin(history->flags));
    break;

  case NODE_TYPE_UNARY:
    codegen_generate_unary(node, history_begin(history->flags));
    break;

  case NODE_TYPE_VARIABLE:
    codegen_generate_scope_variable(node);
    break;

  case NODE_TYPE_STATEMENT_IF:
    codegen_generate_if_stmt(node);
    break;

  case NODE_TYPE_STATEMENT_RETURN:
    codegen_generate_statement_return(node);
    break;
  case NODE_TYPE_STATEMENT_WHILE:
    codegen_generate_while_stmt(node);
    break;
  case NODE_TYPE_STATEMENT_DO_WHILE:
    codegen_generate_do_while_stmt(node);
    break;
  }

  codegen_discard_unused_stack();
}
void codegen_generate_scope_no_new_scope(struct vector *statements,
                                         struct history *history) {
  vector_set_peek_pointer(statements, 0);
  struct node *statement_node = vector_peek_ptr(statements);
  while (statement_node) {
    codegen_generate_statement(statement_node, history);
    statement_node = vector_peek_ptr(statements);
  }
}
void codegen_generate_stack_scope(struct vector *statements, size_t scope_size,
                                  struct history *history) {
  codegen_new_scope(RESOLVER_SCOPE_FLAG_IS_STACK);
  codegen_generate_scope_no_new_scope(statements, history);
  codegen_finish_scope();
}

void codegen_generate_body(struct node *node, struct history *history) {
  codegen_generate_stack_scope(node->body.statements, node->body.size, history);
}
void codegen_generate_function_with_body(struct node *node) {
  codegen_register_function(node, 0);
  asm_push("global %s", node->func.name);
  asm_push("; %s function", node->func.name);
  asm_push("%s:", node->func.name);

  asm_push_ebp();
  asm_push("mov ebp, esp");
  codegen_stack_sub(C_ALIGN(function_node_stack_size(node)));
  codegen_new_scope(RESOLVER_DEFAULT_ENTITY_FLAG_IS_LOCAL_STACK);
  codegen_generate_function_arguments(function_node_argument_vec(node));

  codegen_generate_body(node->func.body_n, history_begin(IS_ALONE_STATEMENT));
  codegen_finish_scope();
  codegen_stack_add(C_ALIGN(function_node_stack_size(node)));
  asm_pop_ebp();
  stackframe_assert_empty(current_function);
  asm_push("ret");
}
void codegen_generate_function(struct node *node) {
  current_function = node;
  if (function_node_is_prototype(node)) {
    codegen_generate_function_prototype(node);
    return;
  }

  codegen_generate_function_with_body(node);
}

void codegen_generate_root_node(struct node *node) {
  switch (node->type) {
  case NODE_TYPE_VARIABLE:
    // We processed this earlier in data section.
    break;

  case NODE_TYPE_FUNCTION:
    codegen_generate_function(node);
    break;
  }
}

void codegen_generate_root() {
  asm_push("section .text");
  struct node *node = NULL;
  while ((node = codegen_node_next()) != NULL) {
    codegen_generate_root_node(node);
  }
}

bool codegen_write_string_char_escaped(char c) {
  const char *c_out = NULL;
  switch (c) {
  case '\n':
    c_out = "10";
    break;

  case '\t':
    c_out = "9";
    break;
  }

  if (c_out) {
    asm_push_no_nl("%s, ", c_out);
  }
  return c_out != NULL;
}
void codegen_write_string(struct string_table_element *element) {
  asm_push_no_nl("%s: db ", element->label);
  size_t len = strlen(element->str);
  for (int i = 0; i < len; i++) {
    char c = element->str[i];
    bool handled = codegen_write_string_char_escaped(c);
    if (handled) {
      continue;
    }
    asm_push_no_nl("'%c', ", c);
  }

  asm_push_no_nl("0");
  asm_push("");
}

void codegen_write_strings() {
  struct code_generator *generator = current_process->generator;
  vector_set_peek_pointer(generator->string_table, 0);
  struct string_table_element *element =
      vector_peek_ptr(generator->string_table);
  while (element) {
    codegen_write_string(element);
    element = vector_peek_ptr(generator->string_table);
  }
}

void codegen_generate_rod() {
  asm_push("section .rodata");
  codegen_write_strings();
}

int codegen(struct compile_process *process) {
  current_process = process;
  scope_create_root(process);
  vector_set_peek_pointer(process->node_tree_vec, 0);
  codegen_new_scope(0);
  codegen_generate_data_section();
  vector_set_peek_pointer(process->node_tree_vec, 0);
  codegen_generate_root();
  codegen_finish_scope();

  // Generate read only data
  codegen_generate_rod();

  return 0;
}
