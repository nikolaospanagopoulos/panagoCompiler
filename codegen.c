#include "compileProcess.h"
#include "compiler.h"
#include "node.h"
#include "vector.h"
#include <complex.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct compileProcess *currentProcess = NULL;

void codegenNewScope(int flags) {
  resolverDefaultNewScope(currentProcess->resolver, flags);
}

void codegenFinishScope() {
  resolverDefaultFinishScope(currentProcess->resolver);
}
int codegenLabelCount();
const char *codegenRegisterString(const char *str);
static struct node *currentFunction = NULL;

static struct history {
  int flags;
} history;

enum {
  RESPONSE_FLAG_ACKNOWLEDGED = 0b00000001,
  RESPONSE_FLAG_PUSHED_STRUCTURE = 0b00000010,
  RESPONSE_FLAG_RESOLVED_ENTITY = 0b00000100,
  RESPONSE_FLAG_UNARY_GET_ADDRESS = 0b00001000,
};
#define RESPONSE_SET(x) &(struct response{x})
#define RESPONSE_EMPTY_RESPONSE_SET()

struct responseData {
  union {
    struct resolverEntity *resolvedEntity;
  };
};

struct response {
  int flags;
  struct responseData data;
};

void codegenResponseExpect() {
  struct response *res = calloc(1, sizeof(struct response));
  vector_push(currentProcess->generator->responses, &res);
}

struct responseData *codegenResponseData(struct response *res) {
  return &res->data;
}

struct response *codegenResponsePull() {
  struct response *res =
      vector_back_ptr_or_null(currentProcess->generator->responses);
  if (res) {
    vector_pop(currentProcess->generator->responses);
  }
  return res;
}
void codegenResponseAknowledge(struct response *responseIn) {
  struct response *res =
      vector_back_or_null(currentProcess->generator->responses);
  if (res) {
    res->flags |= responseIn->flags;
    if (responseIn->data.resolvedEntity) {
      res->data.resolvedEntity = responseIn->data.resolvedEntity;
    }
    res->flags |= RESPONSE_FLAG_ACKNOWLEDGED;
  }
}
bool codegenResponseAknowledged(struct response *res) {
  return res && res->flags & RESPONSE_FLAG_ACKNOWLEDGED;
}

bool codegenResponseHasEntity(struct response *res) {
  return codegenResponseAknowledged(res) &&
         res->flags & RESPONSE_FLAG_RESOLVED_ENTITY && res->data.resolvedEntity;
}

static struct history *historyBegin(int flags) {
  struct history *history = calloc(1, sizeof(struct history));
  history->flags = flags;
  vector_push(currentProcess->gb, &history);
  return history;
}
static struct history *historyDown(struct history *hs, int flags) {
  struct history *newHistory = calloc(1, sizeof(history));
  memcpy(newHistory, hs, sizeof(history));
  newHistory->flags = flags;
  vector_push(currentProcess->gb, &newHistory);
  return newHistory;
}
void asmPushArgs(const char *ins, va_list args) {
  va_list args2;
  va_copy(args2, args);
  vfprintf(stdout, ins, args);
  fprintf(stdout, "\n");
  if (currentProcess->outFile) {
    vfprintf(currentProcess->outFile, ins, args2);
    fprintf(currentProcess->outFile, "\n");
  }
}

void asmPush(const char *ins, ...) {
  va_list args;
  va_start(args, ins);
  asmPushArgs(ins, args);
  va_end(args);
}
void asmPushNoNl(const char *ins, ...) {
  va_list args;
  va_start(args, ins);
  vfprintf(stdout, ins, args);
  va_end(args);
  if (currentProcess->outFile) {
    va_list args;
    va_start(args, ins);
    vfprintf(currentProcess->outFile, ins, args);
    va_end(args);
  }
}

int asmPushInsPop(const char *fmt, int expectingStackEntityType,
                  const char *expectingStackEntityName, ...) {
  char tmpBuf[200];
  sprintf(tmpBuf, "pop %s", fmt);
  va_list args;
  va_start(args, expectingStackEntityName);
  asmPushArgs(tmpBuf, args);
  va_end(args);
  if (!currentFunction) {
    compilerError(currentProcess, "Function doesn't exist \n");
  }
  struct stackFrameElement *element = stackframeBack(currentFunction);
  int flags = element->flags;
  stackFramePopExpecting(currentFunction, expectingStackEntityType,
                         expectingStackEntityName);
  return flags;
}

void asmPushInsPush(const char *fmt, int stackEntityType,
                    const char *stackEntityName, ...) {
  char tmpBuf[200];
  sprintf(tmpBuf, "push %s", fmt);
  va_list args;
  va_start(args, stackEntityName);
  asmPushArgs(tmpBuf, args);
  va_end(args);

  if (!currentFunction) {
    compilerError(currentProcess, "Function doesn't exist \n");
  }
  stackframePush(currentFunction,
                 &(struct stackFrameElement){.type = stackEntityType,
                                             .name = stackEntityName});
}
void asmPushEbp() {
  asmPushInsPush("ebp", STACK_FRAME_ELEMENT_TYPE_SAVED_BP,
                 "functionEntrySavedEbp");
}

void asmPopEbp() {
  asmPushInsPop("ebp", STACK_FRAME_ELEMENT_TYPE_SAVED_BP,
                "functionEntrySavedEbp");
}
void asmPushInsPushWithData(const char *fmt, int stackEntityType,
                            const char *stackEntityName, int flags,
                            struct stackFrameData *data, ...) {

  char tmpBuff[200];
  sprintf(tmpBuff, "push %s", fmt);
  va_list args;
  va_start(args, data);
  asmPushArgs(tmpBuff, args);
  va_end(args);
  flags |= STACK_FRAME_ELEMENT_FLAG_HAS_DATATYPE;
  if (!currentFunction) {
    compilerError(currentProcess, "Not in a function \n");
  }
  stackframePush(currentFunction,
                 &(struct stackFrameElement){.type = stackEntityType,
                                             .name = stackEntityName,
                                             .flags = flags,
                                             .data = *data});
}
static const char *asmKeywordForSize(size_t size, char *tmpBuff) {
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
    sprintf(tmpBuff, "times %ld db ", (unsigned long)size);
    return tmpBuff;
  }
  strcpy(tmpBuff, keyword);
  return tmpBuff;
}
struct node *codegenNextNode() {
  return vector_peek_ptr(currentProcess->nodeTreeVec);
}
void codegenGlobalVariablePrimitive(struct node *node) {
  char tmpBuff[256];
  if (node->var.val != NULL) {
    if (node->var.val->type == NODE_TYPE_STRING) {
      const char *label = codegenRegisterString(node->var.val->sval);
      asmPush("%s: %s %s", node->var.name,
              asmKeywordForSize(variableSize(node), tmpBuff), label);
    } else {
      asmPush("%s: %s %lld", node->var.name,
              asmKeywordForSize(variableSize(node), tmpBuff),
              node->var.val->llnum);
    }
  } else {
    asmPush("%s: %s 0", node->var.name,
            asmKeywordForSize(variableSize(node), tmpBuff));
  }
}
void codegenGenerateGlobalVariable(struct node *node) {
  asmPush("; type: %s, name: %s\n", node->var.type.typeStr, node->var.name);
  switch (node->var.type.type) {
  case DATA_TYPE_VOID:
  case DATA_TYPE_CHAR:
  case DATA_TYPE_SHORT:
  case DATA_TYPE_INTEGER:
  case DATA_TYPE_LONG:
    codegenGlobalVariablePrimitive(node);
    break;
  case DATA_TYPE_DOUBLE:
  case DATA_TYPE_FLOAT:
    compilerError(currentProcess,
                  "Double and Float types are not supported \n");
    break;
  }
}
void generateDataSectionPart(struct node *node) {
  switch (node->type) {
  case NODE_TYPE_VARIABLE:
    codegenGenerateGlobalVariable(node);
    break;
  default:
    break;
  }
}
void generateDataSection() {
  asmPush("section .data");
  struct node *node = codegenNextNode();
  while (node) {
    generateDataSectionPart(node);
    node = codegenNextNode();
  }
}
const char *codegenGetLabelForString(const char *str) {
  const char *result = NULL;
  struct codeGenerator *gen = currentProcess->generator;
  vector_set_peek_pointer(gen->stringTable, 0);
  struct stringTableElement *current = vector_peek_ptr(gen->stringTable);
  while (current) {
    if (S_EQ(current->str, str)) {
      result = current->label;
      break;
    }
    current = vector_peek_ptr(gen->stringTable);
  }
  return result;
}
const char *codegenRegisterString(const char *str) {
  const char *label = codegenGetLabelForString(str);
  if (label) {
    // already registered this string
    return label;
  }
  struct stringTableElement *strElem =
      calloc(1, sizeof(struct stringTableElement));
  int labelId = codegenLabelCount();
  sprintf((char *)strElem->label, "str_%i", labelId);
  strElem->str = str;
  vector_push(currentProcess->generator->stringTable, &strElem);
  return strElem->label;
}
bool codegenWriteCharEscapedString(char c) {
  const char *cout = NULL;
  switch (c) {
  case '\n':
    cout = "10";
    break;
  case '\t':
    cout = "9";
    break;
  }
  if (cout) {
    asmPushNoNl("%s, ", cout);
  }
  return cout != NULL;
}
void codegenWriteString(struct stringTableElement *element) {
  asmPushNoNl("%s: db ", element->label);
  size_t len = strlen(element->str);
  for (int i = 0; i < len; i++) {
    char c = element->str[i];
    bool handled = codegenWriteCharEscapedString(c);
    if (handled) {
      continue;
    }
    asmPushNoNl("'%c', ", c);
  }
  asmPushNoNl("0");
  asmPush("");
}
void codegenWriteStrings() {
  struct codeGenerator *gen = currentProcess->generator;
  vector_set_peek_pointer(gen->stringTable, 0);
  struct stringTableElement *element = vector_peek_ptr(gen->stringTable);
  while (element) {
    codegenWriteString(element);
    element = vector_peek_ptr(gen->stringTable);
  }
}
void generateRod() {
  asmPush("section .rodata");
  codegenWriteStrings();
}

struct resolverEntity *codegenRegisterFunction(struct node *funcNode,
                                               int flags) {
  return resolverDefaultRegisterFunction(currentProcess->resolver, funcNode,
                                         flags);
}

void codegenGenerateFunctionPrototype(struct node *node) {
  codegenRegisterFunction(node, 0);
  asmPush("extern %s", node->func.name);
}

void codegenStackSubWithName(size_t stackSize, const char *name) {
  if (stackSize != 0) {
    stackframeSub(currentFunction, STACK_FRAME_ELEMENT_TYPE_UNKNOWN, name,
                  stackSize);
    asmPush("sub esp %lld", stackSize);
  }
}
void codegenStackAddWithName(size_t stackSize, const char *name) {
  if (stackSize != 0) {
    stackframeAdd(currentFunction, STACK_FRAME_ELEMENT_TYPE_UNKNOWN, name,
                  stackSize);
    asmPush("add esp %lld", stackSize);
  }
}

void codegenStackAdd(size_t size) {
  codegenStackAddWithName(size, "stack_change");
}

void codegenStackSub(size_t stackSize) {
  codegenStackSubWithName(stackSize, "stack_change");
}

struct resolverEntity *codegenNewScopeEntity(struct node *varNode, int offset,
                                             int flags) {
  return resolverDefaultNewScopeEntity(currentProcess->resolver, varNode,
                                       offset, flags);
}

void codegenGenerateFunctionArguments(struct vector *argumentVector) {
  vector_set_peek_pointer(argumentVector, 0);
  struct node *current = vector_peek_ptr(argumentVector);
  // TODO: cleanup
  while (current) {
    codegenNewScopeEntity(current, current->var.aoffset,
                          RESOLVER_DEFAULT_ENTITY_FLAG_IS_LOCAL_STACK);
    current = vector_peek_ptr(argumentVector);
  }
}
void codegenGenerateNumberNode(struct node *node, struct history *history) {
  asmPushInsPushWithData(
      "dword %i", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value",
      STACK_FRAME_ELEMENT_FLAG_IS_NUMERICAL,
      &(struct stackFrameData){.dtype = datatypeForNumeric()}, node->llnum);
}
bool codegenIsExpRootForFlags(int flags) {
  return !(flags & EXPRESSION_IS_NOT_ROOT_NODE);
}
bool codegenIsExpRoot(struct history *history) {
  return codegenIsExpRootForFlags(history->flags);
}
void codegenGenerateExpressionable(struct node *node, struct history *history) {
  bool isRoot = codegenIsExpRoot(history);
  if (isRoot) {
    history->flags |= EXPRESSION_IS_NOT_ROOT_NODE;
  }

  switch (node->type) {
  case NODE_TYPE_NUMBER:
    codegenGenerateNumberNode(node, history);
    break;
  }
}
const char *codegenSubRegister(const char *originalRegister, size_t size) {
  const char *reg = NULL;
  if (S_EQ(originalRegister, "eax")) {
    if (size == DATA_SIZE_BYTE) {
      reg = "al";
    } else if (size == DATA_SIZE_WORD) {
      reg = "ax";
    } else if (size == DATA_SIZE_DWORD) {
      reg = "eax";
    } else if (size == DATA_SIZE_DDWORD) {
      reg = "rax";
    }
  } else if (S_EQ(originalRegister, "ebx")) {
    if (size == DATA_SIZE_BYTE) {
      reg = "bl";
    } else if (size == DATA_SIZE_WORD) {
      reg = "bx";
    } else if (size == DATA_SIZE_DWORD) {
      reg = "ebx";
    } else if (size == DATA_SIZE_DDWORD) {
      reg = "rbx";
    }
  } else if (S_EQ(originalRegister, "ecx")) {
    if (size == DATA_SIZE_BYTE) {
      reg = "cl";
    } else if (size == DATA_SIZE_WORD) {
      reg = "cx";
    } else if (size == DATA_SIZE_DWORD) {
      reg = "ecx";
    } else if (size == DATA_SIZE_DDWORD) {
      reg = "rcx";
    }
  } else if (S_EQ(originalRegister, "edx")) {
    if (size == DATA_SIZE_BYTE) {
      reg = "dl";
    } else if (size == DATA_SIZE_WORD) {
      reg = "dx";
    } else if (size == DATA_SIZE_DWORD) {
      reg = "edx";
    } else if (size == DATA_SIZE_DDWORD) {
      reg = "rdx";
    }
  }
  return reg;
}
const char *codegenByteWordOrDwordOrDDword(size_t size, const char **regToUse) {
  const char *type = NULL;
  const char *newRegister = *regToUse;
  if (size == DATA_SIZE_BYTE) {
    type = "byte";
    newRegister = codegenSubRegister(*regToUse, DATA_SIZE_BYTE);
  } else if (size == DATA_SIZE_WORD) {
    type = "word";
    newRegister = codegenSubRegister(*regToUse, DATA_SIZE_WORD);
  } else if (size == DATA_SIZE_DWORD) {
    type = "dword";
    newRegister = codegenSubRegister(*regToUse, DATA_SIZE_DWORD);
  } else if (size == DATA_SIZE_DDWORD) {
    type = "ddword";
    newRegister = codegenSubRegister(*regToUse, DATA_SIZE_DDWORD);
  }
  *regToUse = newRegister;
  return type;
}
struct resolverDefaultEntityData *
codegenEntityPrivate(struct resolverEntity *entity) {
  return resolverDefaultEntityPrivate(entity);
}
void codegenGenerateAssignmentInstructionForOperator(const char *movTypeKeyword,
                                                     const char *address,
                                                     const char *regToUse,
                                                     const char *operator,
                                                     bool isSigned) {
  if (S_EQ(operator, "=")) {
    asmPush("mov %s [%s], %s", movTypeKeyword, address, regToUse);
  } else if (S_EQ(operator, "+=")) {
    asmPush("add %s [%s], %s", movTypeKeyword, address, regToUse);
  }
}
void codegenGenerateScopeVariable(struct node *node) {
  struct resolverEntity *entity = codegenNewScopeEntity(
      node, node->var.aoffset, RESOLVER_DEFAULT_ENTITY_FLAG_IS_LOCAL_STACK);
  if (node->var.val) {
    codegenGenerateExpressionable(node->var.val,
                                  historyBegin(EXPRESSION_IS_ASSIGNMENT |
                                               IS_RIGHT_OPERAND_OF_ASSIGNMENT));
    asmPushInsPop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    const char *regToUse = "eax";
    const char *movType = codegenByteWordOrDwordOrDDword(
        datatypeElementSize(&entity->dtype), &regToUse);
    codegenGenerateAssignmentInstructionForOperator(
        movType, codegenEntityPrivate(entity)->address, regToUse, "=",
        entity->dtype.flags & DATATYPE_FLAG_IS_SIGNED);
  }
}
void codegenGenerateEntityAccessStart(
    struct resolverResult *result, struct resolverEntity *rootAssignmentEntity,
    struct history *history) {
  if (rootAssignmentEntity->type == RESOLVER_ENTITY_TYPE_UNSUPPORTED) {
    codegenGenerateExpressionable(rootAssignmentEntity->node, history);
  } else if (result->flags & RESOLVER_RESULT_FLAG_FIRST_ENTITY_PUSH_VALUE) {
    asmPushInsPushWithData(
        "dword [%s]", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
        &(struct stackFrameData){.dtype = rootAssignmentEntity->dtype},
        result->base.address);
  } else if (result->flags & RESOLVER_RESULT_FLAG_FIRST_ENTITY_LOAD_TO_EBX) {
    if (rootAssignmentEntity->next &&
        rootAssignmentEntity->next->flags &
            RESOLVER_ENTITY_FLAG_IS_POINTER_ARRAY_ENTITY) {
      asmPush("mov ebx, [%s]", result->base.address);
    } else {
      asmPush("lea ebx, [%s]", result->base.address);
    }
    asmPushInsPushWithData(
        "ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
        &(struct stackFrameData){.dtype = rootAssignmentEntity->dtype});
  }
}
void codegenGenerateEntityAccessForVariableOrGeneral(
    struct resolverResult *result, struct resolverEntity *entity) {
  asmPushInsPop("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
  if (entity->flags & RESOLVER_ENTITY_FLAG_DO_INDIRECTION) {
    asmPush("mov ebx, [ebx]");
  }
  asmPush("add ebx, %i", entity->offset);
  asmPushInsPushWithData("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE,
                         "result_value", 0,
                         &(struct stackFrameData){.dtype = entity->dtype});
}
void codegenGenerateEntityAccessForEntityAssignmentLeftOperand(
    struct resolverResult *result, struct resolverEntity *entity,
    struct history *history) {
  switch (entity->type) {

  case RESOLVER_ENTITY_TYPE_ARRAY_BRACKET:
    break;
  case RESOLVER_ENTITY_TYPE_VARIABLE:
  case RESOLVER_ENTITY_TYPE_GENERAL:
    codegenGenerateEntityAccessForVariableOrGeneral(result, entity);
    break;
  case RESOLVER_ENTITY_TYPE_FUNCTION_CALL:
    break;
  case RESOLVER_ENTITY_TYPE_UNARY_INDIRECTION:
    break;
  case RESOLVER_ENTITY_TYPE_UNARY_GET_ADDRESS:
    break;
  case RESOLVER_ENTITY_TYPE_UNSUPPORTED:
    break;
  case RESOLVER_ENTITY_TYPE_CAST:
    break;
  default:
    compilerError(currentProcess, "CompilerBug\n");
  }
}

void codegenGenerateEntityAccessForAssignmentLeftOperand(
    struct resolverResult *result, struct resolverEntity *rootAssignmentEntity,
    struct node *topMostNode, struct history *history) {
  codegenGenerateEntityAccessStart(result, rootAssignmentEntity, history);
  struct resolverEntity *current =
      resolverResultEntityNext(rootAssignmentEntity);
  while (current) {

    codegenGenerateEntityAccessForEntityAssignmentLeftOperand(result, current,
                                                              history);
    resolverResultEntityNext(current);
  }
}
void codegenGenerateAssignmentPart(struct node *node, const char *op,
                                   struct history *history) {
  struct datatype rightOperandDtype;
  struct resolverResult *result =
      resolverFollow(currentProcess->resolver, node);
  if (!resolverResultOK(result)) {
    compilerError(currentProcess,
                  "Something went wrong with the resolver result \n");
  }
  struct resolverEntity *rootAssignmentEntity =
      resolverResultEntityRoot(result);
  const char *regToUse = "eax";
  const char *movType = codegenByteWordOrDwordOrDDword(
      datatypeElementSize(&result->lastEntity->dtype), &regToUse);
  struct resolverEntity *nextEntity =
      resolverResultEntityNext(rootAssignmentEntity);
  if (!nextEntity) {
    if (datatypeIsStructOrUnionNotPtr(&result->lastEntity->dtype)) {

    } else {
      asmPushInsPop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE,
                    "result_value");
      codegenGenerateAssignmentInstructionForOperator(
          movType, result->base.address, regToUse, op,
          result->lastEntity->dtype.flags & DATATYPE_FLAG_IS_SIGNED);
    }
  } else {
    codegenGenerateEntityAccessForAssignmentLeftOperand(
        result, rootAssignmentEntity, node, history);
    asmPushInsPop("edx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    asmPushInsPop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    codegenGenerateAssignmentInstructionForOperator(
        movType, "edx", regToUse, op,
        result->lastEntity->flags & DATATYPE_FLAG_IS_SIGNED);
  }
}
void codegenGenerateAssignmentExpression(struct node *node,
                                         struct history *history) {
  codegenGenerateExpressionable(
      node->exp.right,
      historyDown(history,
                  EXPRESSION_IS_ASSIGNMENT | IS_RIGHT_OPERAND_OF_ASSIGNMENT));
  codegenGenerateAssignmentPart(node->exp.left, node->exp.op, history);
}
void codegenGenerateEntityAccessForEntity(struct resolverResult *result,
                                          struct resolverEntity *entity,
                                          struct history *history) {

  switch (entity->type) {

  case RESOLVER_ENTITY_TYPE_ARRAY_BRACKET:
    break;
  case RESOLVER_ENTITY_TYPE_VARIABLE:
  case RESOLVER_ENTITY_TYPE_GENERAL:
    codegenGenerateEntityAccessForVariableOrGeneral(result, entity);
    break;
  case RESOLVER_ENTITY_TYPE_FUNCTION_CALL:
    break;
  case RESOLVER_ENTITY_TYPE_UNARY_INDIRECTION:
    break;
  case RESOLVER_ENTITY_TYPE_UNARY_GET_ADDRESS:
    break;
  case RESOLVER_ENTITY_TYPE_UNSUPPORTED:
    break;
  case RESOLVER_ENTITY_TYPE_CAST:
    break;
  default:
    compilerError(currentProcess, "CompilerBug\n");
  }
}
void codegenGenerateEntityAccess(struct resolverResult *result,
                                 struct resolverEntity *rootAssignmentEntity,
                                 struct node *topMostNode,
                                 struct history *history) {
  codegenGenerateEntityAccessStart(result, rootAssignmentEntity, history);
  struct resolverEntity *current =
      resolverResultEntityNext(rootAssignmentEntity);
  while (current) {
    codegenGenerateEntityAccessForEntity(result, current, history);
    current = resolverResultEntityNext(current);
  }
}
bool codegenResolveNodeReturnResult(struct node *node, struct history *history,
                                    struct resolverResult **resultOut) {
  struct resolverResult *result =
      resolverFollow(currentProcess->resolver, node);
  if (resolverResultOK(result)) {
    struct resolverEntity *rootAssignmentEntity =
        resolverResultEntityRoot(result);
    codegenGenerateEntityAccess(result, rootAssignmentEntity, node, history);
    if (resultOut) {
      *resultOut = result;
    }
    codegenResponseAknowledge(
        &(struct response){.flags = RESPONSE_FLAG_RESOLVED_ENTITY,
                           .data.resolvedEntity = result->lastEntity});
    return true;
  }
  return false;
}
bool codegenResolveNodeForValue(struct node *node, struct history *history) {
  struct resolverResult *result = NULL;
  if (!codegenResolveNodeReturnResult(node, history, &result)) {
  }
}
int getAdditionalFlags(int currentFlags, struct node *node) {
  if (node->type != NODE_TYPE_EXPRESSION) {
    return 0;
  }
  int additionalFlags = 0;
  bool maintainFunctionCallArgumentFlag =
      (currentFlags & EXPRESSION_IN_FUNCTION_CALL_ARGUMENTS) &&
      S_EQ(node->exp.op, ",");
  if (maintainFunctionCallArgumentFlag) {
    additionalFlags |= EXPRESSION_IN_FUNCTION_CALL_ARGUMENTS;
  }
  return additionalFlags;
}
int codegenSetFlagForOperator(const char *op) {
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
void codegenGenerateExpNodeForArithmentic(struct node *node,
                                          struct history *history) {
  if (node->type != NODE_TYPE_EXPRESSION) {
    compilerError(currentProcess, "Not an expression \n");
  }
  int flags = history->flags;
  /*
  if(isLogicalOperator(node->exp.op)){

  }
  */
  struct node *leftNode = node->exp.left;
  struct node *rightNode = node->exp.right;

  int opFlags = codegenSetFlagForOperator(node->exp.op);
}
void codegenGenerateExpNode(struct node *node, struct history *history) {
  if (isNodeAssignment(node)) {
    codegenGenerateAssignmentExpression(node, history);
    return;
  }
  if (codegenResolveNodeForValue(node, history)) {
    return;
  }
  int additionalFlags = getAdditionalFlags(history->flags, node);

  codegenGenerateExpNodeForArithmentic();
}
void codegenGenerateStatement(struct node *node, struct history *history) {
  switch (node->type) {
  case NODE_TYPE_EXPRESSION:
    codegenGenerateExpNode(node, historyBegin(history->flags));
    break;
  case NODE_TYPE_VARIABLE:
    codegenGenerateScopeVariable(node);
    break;
  }
}
void codegenGenerateScopeNoNewScope(struct vector *statements,
                                    struct history *history) {
  vector_set_peek_pointer(statements, 0);
  struct node *statementNode = vector_peek_ptr(statements);
  while (statementNode) {
    codegenGenerateStatement(statementNode, history);
    statementNode = vector_peek_ptr(statements);
  }
}
void codegenGenerateStackScope(struct vector *statements, size_t scopeSize,
                               struct history *history) {
  codegenNewScope(RESOLVER_SCOPE_FLAG_IS_STACK);
  codegenGenerateScopeNoNewScope(statements, history);
  codegenFinishScope();
}
void codegenGenerateBody(struct node *node, struct history *history) {

  codegenGenerateStackScope(node->body.statements, node->body.size, history);
}

void codegenGenerateFunctionWithBody(struct node *node) {
  codegenRegisterFunction(node, 0);
  asmPush("global %s", node->func.name);
  asmPush("; %s function", node->func.name);
  asmPush("%s:", node->func.name);

  asmPushEbp();
  asmPush("mov ebp, esp");
  codegenStackSub(C_ALIGN(functionNodeStackSize(node, currentProcess)));
  // TODO: cleanup
  codegenNewScope(RESOLVER_DEFAULT_ENTITY_FLAG_IS_LOCAL_STACK);
  codegenGenerateFunctionArguments(functionNodeArgumentVec(node));
  // TODO: cleanup
  codegenGenerateBody(node->func.bodyN, historyBegin(IS_ALONE_STATEMENT));
  codegenFinishScope();
  codegenStackAdd(C_ALIGN(functionNodeStackSize(node, currentProcess)));
  asmPopEbp();
  stackFrameAssertEmpty(currentFunction);
  asmPush("ret");
}

struct vector *functionNodeArgumentVec(struct node *node) {
  if (node->type != NODE_TYPE_FUNCTION) {
    compilerError(currentProcess, "Not a function node \n");
  }

  return node->func.args.vector;
}

void codegenGenerateFunction(struct node *node) {
  currentFunction = node;
  if (functionNodeIsPrototype(node)) {
    codegenGenerateFunctionPrototype(node);
    return;
  }
  codegenGenerateFunctionWithBody(node);
}
void codegenGenerateRootNode(struct node *node) {
  switch (node->type) {
  case NODE_TYPE_VARIABLE:
    // processed earlied in .data section
    break;
  case NODE_TYPE_FUNCTION:
    codegenGenerateFunction(node);
    break;
  }
}
void codegenGenerateRoot() {
  asmPush("section .text");
  struct node *node = NULL;
  while ((node = codegenNextNode()) != NULL) {
    codegenGenerateRootNode(node);
  }
}
struct codeGenerator *codegenNew(struct compileProcess *process) {
  struct codeGenerator *generator = calloc(1, sizeof(struct codeGenerator));
  generator->entryPoints = vector_create(sizeof(struct codegenEntryPoint *));
  generator->exitPoints = vector_create(sizeof(struct codegenExitPoint *));
  generator->responses = vector_create(sizeof(struct response *));
  generator->stringTable = vector_create(sizeof(struct stringTableElement *));
  return generator;
}
void codeGeneratorRegisterExitPoint(int exitPointId) {
  struct codeGenerator *gen = currentProcess->generator;
  struct codegenExitPoint *exitPoint =
      calloc(1, sizeof(struct codegenExitPoint));
  exitPoint->id = exitPointId;
  vector_push(gen->exitPoints, &exitPoint);
}

void codeGeneratorRegisterEntryPoint(int entryPointId) {
  struct codeGenerator *gen = currentProcess->generator;
  struct codegenEntryPoint *entryPoint =
      calloc(1, sizeof(struct codegenEntryPoint));
  entryPoint->id = entryPointId;
  vector_push(gen->entryPoints, &entryPoint);
}
int codegenLabelCount() {
  static int count = 0;
  count++;
  return count;
}

struct codegenExitPoint *codegenCurrentExitPoint() {
  struct codeGenerator *gen = currentProcess->generator;
  return vector_back_ptr_or_null(gen->exitPoints);
}
struct codegenEntryPoint *codegenCurrentEntryPoint() {
  struct codeGenerator *gen = currentProcess->generator;
  return vector_back_ptr_or_null(gen->entryPoints);
}
void codegenBeginExitPoint() {
  int exitPointId = codegenLabelCount();
  codeGeneratorRegisterExitPoint(exitPointId);
}

void codegenBeginEntryPoint() {
  int entryPointId = codegenLabelCount();
  codeGeneratorRegisterEntryPoint(entryPointId);
  asmPush(".entry_point_%i:", entryPointId);
}
void codegenEndEntryPoint() {
  struct codeGenerator *gen = currentProcess->generator;
  struct codegenEntryPoint *entryPoint = codegenCurrentEntryPoint();
  if (!entryPoint) {
    compilerError(currentProcess, "no entry point to end \n");
  }
  free(entryPoint);
  vector_pop(gen->entryPoints);
}
void codegenBeginEntryExitPoint() {
  codegenBeginEntryPoint();
  codegenBeginExitPoint();
}

void codegenEndExitPoint() {
  struct codeGenerator *gen = currentProcess->generator;
  struct codegenExitPoint *exitPoint = codegenCurrentExitPoint();
  if (!exitPoint) {
    compilerError(currentProcess, "exit point doesnt exist \n");
  }
  asmPush(".exit_point_%i:", exitPoint->id);
  free(exitPoint);
  vector_pop(gen->exitPoints);
}
void codegenEndEntryExitPoint() {
  codegenEndEntryPoint();
  codegenEndExitPoint();
}
void codegenGotoEntryPoint(struct node *node) {
  struct codeGenerator *gen = currentProcess->generator;
  struct codegenEntryPoint *entryPoint = codegenCurrentEntryPoint();
  asmPush("jmp .entry_point_%i", entryPoint->id);
}
void codegenGoToExitPoint(struct node *node) {
  struct codeGenerator *gen = currentProcess->generator;
  struct codegenExitPoint *exitPoint = codegenCurrentExitPoint();
  asmPush("jmp .exit_point_%i", exitPoint->id);
}
int codegen(struct compileProcess *process) {
  currentProcess = process;
  createRootScope(process);
  vector_set_peek_pointer(process->nodeTreeVec, 0);
  codegenNewScope(0);
  generateDataSection();
  vector_set_peek_pointer(process->nodeTreeVec, 0);
  codegenGenerateRoot();
  codegenFinishScope();
  generateRod();
  return CODEGEN_ALL_OK;
}
