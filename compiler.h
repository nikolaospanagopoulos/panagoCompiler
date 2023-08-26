#pragma once
#include "compileProcess.h"
#include "node.h"
#include <stddef.h>

#define S_EQ(str, str2) (str && str2 && (strcmp(str, str2) == 0))
#define SYMBOL_CASE                                                            \
  case '{':                                                                    \
  case '}':                                                                    \
  case ':':                                                                    \
  case ';':                                                                    \
  case '#':                                                                    \
  case '\\':                                                                   \
  case ')':                                                                    \
  case ']'

#define NUMERIC_CASE                                                           \
  case '0':                                                                    \
  case '1':                                                                    \
  case '2':                                                                    \
  case '3':                                                                    \
  case '4':                                                                    \
  case '5':                                                                    \
  case '6':                                                                    \
  case '7':                                                                    \
  case '8':                                                                    \
  case '9'
#define OPERATOR_CASE_EXCLUDING_DIVISION                                       \
  case '+':                                                                    \
  case '-':                                                                    \
  case '*':                                                                    \
  case '>':                                                                    \
  case '<':                                                                    \
  case '^':                                                                    \
  case '%':                                                                    \
  case '!':                                                                    \
  case '=':                                                                    \
  case '~':                                                                    \
  case '|':                                                                    \
  case '&':                                                                    \
  case '(':                                                                    \
  case '[':                                                                    \
  case ',':                                                                    \
  case '.':                                                                    \
  case '?'
struct lexProcess;
typedef char (*LEX_PROCESS_NEXT_CHAR)(struct lexProcess *process);
typedef char (*LEX_PROCESS_PEEK_CHAR)(struct lexProcess *process);
typedef void (*LEX_PROCESS_PUSH_CHAR)(struct lexProcess *process, char c);

struct lexProcessFunctions {
  LEX_PROCESS_NEXT_CHAR nextChar;
  LEX_PROCESS_PEEK_CHAR peekChar;
  LEX_PROCESS_PUSH_CHAR pushChar;
};

enum { LEX_ALL_OK, LEX_ERROR };

enum {
  INSIDE_EXPRESSION = 0b00000001,
  NODE_FLAG_IS_FORWARD_DECLERATION = 0b00000010,
  NODE_FLAG_HAS_VARIABLE_COMBINED = 0b00000100
};

typedef struct lexProcess {
  pos pos;
  struct vector *tokenVec;
  struct compileProcess *compiler;
  int currentExpressionCount;
  struct buffer *parenthesesBuffer;
  struct lexProcessFunctions *function;
  void *privateData;

} lexProcess;

enum { PARSE_ALL_OK, PARSE_ERROR };
enum { CODEGEN_ALL_OK, CODEGEN_ERROR };

#define TOTAL_OPERATOR_GROUPS 14
#define MAX_OPERATORS_IN_GROUP 12

enum {
  ASSOCIATIVITY_LEFT_TO_RIGHT,
  ASSOCIATIVITY_RIGHT_TO_LEFT,
};

struct expressionableOpPrecedenceGroup {
  char *operators[MAX_OPERATORS_IN_GROUP];
  int associtivity;
};

char compileProcessNextChar(lexProcess *process);
char compileProcessPeekChar(lexProcess *process);
void compileProcessPushChar(lexProcess *process, char c);
int compileFile(const char *filename, const char *outFileName, int flags);
lexProcess *lexProcessCreate(compileProcess *compiler,
                             struct lexProcessFunctions *functions,
                             void *privateData);
void freeLexProcess(lexProcess *process);

void *lexProcessPrivateData(lexProcess *process);
struct vector *lexProcessTokens(lexProcess *process);
int lex(lexProcess *process);
void compilerError(compileProcess *compiler, const char *msg, ...);
void compilerWarning(compileProcess *compiler, const char *msg, ...);
bool tokenIsKeyword(token *token, const char *value);
int parse(compileProcess *process);
node *nodeCreate(node *_node);
node *nodePop();
node *nodePeek();
void nodeSetVector(struct vector *vec, struct vector *rootVec,
                   struct vector *garbageVec, struct vector *garbageForVec);

void nodePush(node *node);

node *nodePeekOrNull();

bool nodeIsExpressionable(node *node);
node *nodeCreate(node *_node);
node *nodePeekExpressionableOrNull();
node *makeExpNode(node *left, node *right, const char *op);
bool keywordIsDataType(const char *str);

enum {
  DATA_TYPE_VOID,
  DATA_TYPE_CHAR,
  DATA_TYPE_SHORT,
  DATA_TYPE_INTEGER,
  DATA_TYPE_LONG,
  DATA_TYPE_FLOAT,
  DATA_TYPE_DOUBLE,
  DATA_TYPE_STRUCT,
  DATA_TYPE_UNION,
  DATA_TYPE_UNKNOWN
};
bool tokenIsPrimitiveKeyword(token *token);
enum {
  DATA_TYPE_EXPECT_PRIMITIVE,
  DATA_TYPE_EXPECT_UNION,
  DATA_TYPE_EXPECT_STRUCT
};
bool datatypeIsStructOrUnionForName(const char *name);
bool tokenIsOperator(token *token, const char *val);
enum {
  DATA_SIZE_ZERO = 0,
  DATA_SIZE_BYTE = 1,
  DATA_SIZE_WORD = 2,
  DATA_SIZE_DWORD = 4,
  DATA_SIZE_DDWORD = 8
};

enum { FUNCTION_NODE_FLAG_IS_NATIVE = 0b00000001 };
struct codegenEntryPoint {
  int id;
};
struct codegenExitPoint {
  int id;
};

struct stringTableElement {
  const char *str;
  const char label[50];
};
struct codeGenerator {
  struct vector *stringTable;
  struct vector *entryPoints;
  struct vector *exitPoints;
};

enum {
  STACK_FRAME_ELEMENT_TYPE_LOCAL_VAR,
  STACK_FRAME_ELEMENT_TYPE_SAVED_REG,
  STACK_FRAME_ELEMENT_TYPE_SAVED_BP,
  STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE,
  STACK_FRAME_ELEMENT_TYPE_UNKNOWN,
};

enum {
  STACK_FRAME_ELEMENT_FLAG_IS_PUSHED_ADDRESS = 0b00000001,
  STACK_FRAME_ELEMENT_FLAG_ELEMENT_NOT_FOUND = 0b00000010,
  STACK_FRAME_ELEMENT_FLAG_IS_NUMERICAL = 0b00000100,
  STACK_FRAME_ELEMENT_FLAG_HAS_DATATYPE = 0b00001000,
};
struct stackFrameData {
  // datatype that was pushed to the stack
  struct datatype dtype;
};

struct stackFrameElement {
  // stack frame element flags
  int flags;
  // type of the frame element
  int type;
  // the name of the frame element. not a var name
  const char *name;
  // the offset this element is on the base ptr
  int offsetFromBasePtr;
  struct stackFrameData data;
};

// Resolver
enum {
  RESOLVER_ENTITY_FLAG_IS_STACK = 0b00000001,
  RESOLVER_ENTITY_FLAG_NO_MERGE_WITH_NEXT_ENTITY = 0b00000010,
  RESOLVER_ENTITY_FLAG_NO_MERGE_WITH_LEFT_ENTITY = 0b00000100,
  RESOLVER_ENTITY_FLAG_DO_INDIRECTION = 0b00001000,
  RESOLVER_ENTITY_FLAG_JUST_USE_OFFSET = 0b00010000,
  RESOLVER_ENTITY_FLAG_IS_POINTER_ARRAY_ENTITY = 0b00100000,
  RESOLVER_ENTITY_FLAG_WAS_CASTED = 0b01000000,
  RESOLVER_ENTITY_FLAG_USES_ARRAY_BRACKETS = 0b10000000
};

enum {
  RESOLVER_ENTITY_TYPE_VARIABLE,
  RESOLVER_ENTITY_TYPE_FUNCTION,
  RESOLVER_ENTITY_TYPE_STRUCTURE,
  RESOLVER_ENTITY_TYPE_FUNCTION_CALL,
  RESOLVER_ENTITY_TYPE_ARRAY_BRACKET,
  RESOLVER_ENTITY_TYPE_RULE,
  RESOLVER_ENTITY_TYPE_GENERAL,
  RESOLVER_ENTITY_TYPE_UNARY_GET_ADDRESS,
  RESOLVER_ENTITY_TYPE_UNARY_INDIRECTION,
  RESOLVER_ENTITY_TYPE_UNSUPPORTED,
  RESOLVER_ENTITY_TYPE_CAST
};

enum { RESOLVER_SCOPE_FLAG_IS_STACK = 0b00000001 };

struct resolverResult;
struct resolverProcess;
struct resolverScope;
struct resolverEntity;
typedef void *(*RESOLVER_NEW_ARRAY_BRACKET_ENTITY)(
    struct resolverResult *result, struct node *arrayEntityNode);
typedef void (*RESOLVER_DELETE_SCOPE)(struct resolverScope *scope);
typedef void (*RESOLVER_DELETE_ENTITY)(struct resolverEntity *entity);
typedef struct resolverEntity *(*RESOLVER_MERGE_ENTITIES)(
    struct resolverProcess *process, struct resolverResult *result,
    struct resolverEntity *leftEntity, struct resolverEntity *rightEntity);
typedef void *(*RESOLVER_MAKE_PRIVATE)(struct resolverEntity *entity,
                                       struct node *node, int offset,
                                       struct resolverScope *scope);
typedef void (*RESOLVER_SET_RESULT_BASE)(struct resolverResult *result,
                                         struct resolverEntity *baseEntity);

struct resolverCallbacks {
  RESOLVER_NEW_ARRAY_BRACKET_ENTITY new_array_entity;
  RESOLVER_DELETE_SCOPE delete_scope;
  RESOLVER_DELETE_ENTITY delete_entity;
  RESOLVER_MERGE_ENTITIES merge_entities;
  RESOLVER_MAKE_PRIVATE make_private;
  RESOLVER_SET_RESULT_BASE set_result_base;
};

struct compileProcess;
struct resolverProcess {
  struct resolverScopes {
    struct resolverScope *root;
    struct resolverScope *current;
  } scope;
  struct compileProcess *cmpProcess;
  struct resolverCallbacks callbacks;
};
struct resolverScope {
  int flags;
  struct vector *entities;
  struct resolverScope *next;
  struct resolverScope *prev;
};

struct resolverArrayData {
  // holds nodes of type resolver entity
  struct vector *arrayEntities;
};
enum {
  RESOLVER_RESULT_FLAG_FAILED = 0b00000001,
  RESOLVER_RESULT_FLAG_RUNTIME_NEEDED_TO_FINISH_PATH = 0b00000010,
  RESOLVER_RESULT_FLAG_PROCESSING_ARRAY_ENTITIES = 0b00000100,
  RESOLVER_RESULT_FLAG_HAS_POINTER_ARRAY_ACCESS = 0b00001000,
  RESOLVER_RESULT_FLAG_FIRST_ENTITY_LOAD_TO_EBX = 0b00010000,
  RESOLVER_RESULT_FLAG_FIRST_ENTITY_PUSH_VALUE = 0b00100000,
  RESOLVER_RESULT_FLAG_FINAL_INDIRECTION_REQUIRED_FOR_VALUE = 0b01000000,
  RESOLVER_RESULT_FLAG_DOES_GET_ADDRESS = 0b10000000
};
struct resolverResult {
  // the first entity in the resolver result
  struct resolverEntity *firstEntityConst;

  // this entity represents the variable at the  start of this expression
  struct resolverEntity *identifier;

  // equal to the last structure or union entity discovered
  struct resolverEntity *lastStructUnionEntity;

  struct resolverArrayData arrayData;

  // The root entity
  struct resolverEntity *entity;
  struct resolverEntity *lastEntity;
  int flags;

  // total number of enitities
  size_t count;

  struct resolverResultBase {
    //[ebp - 4] , [name + 4]
    char address[60];
    // EBP, global var
    char baseAddress[60];
    // 4 || -4
    int offset;
  } base;
};
struct resolverEntity {
  int type;
  int flags;

  // the name of the resolved entity
  const char *name;

  // the offset from the base ptr
  int offset;

  // the node the entiry is relating to
  struct node *node;

  union {
    struct resolverEntityVarData {
      struct datatype dtype;
      struct resolverArrayRuntime {
        struct datatype dtype;
        struct node *indexNode;
        int multiplier;
      } arrayRuntime;
    } varData;
    struct resolverArray {
      struct datatype dtype;
      int multiplier;
      struct node *arrayIndexNode;
      int index;
    } array;
    struct resolverEntityFunctionCallData {
      struct vector *arguments;
      // total size used by the function call;
      size_t stackSize;
    } funcCallData;
    struct resolverEntityRule {
      struct resolverEntityRuleLeft {
        int flags;
      } left;
      struct resolverEntityRuleRight {
        int flags;
      } right;
    } rule;
    struct resolverIndirection {
      // how much is the depth we need to find the value
      int depth;
    } indirection;
  };
  struct entityLastReolve {
    struct node *referencingNode;

  } lastResolve;
  // datatype of the resolver entity
  struct datatype dtype;
  // the scope this entity belongs to
  struct resolverScope *scope;
  // the result of the resolution
  struct resolverResult *result;
  // the resolved process
  struct resolverProcess *process;
  void *privateData;
  // linked list
  struct resolverEntity *next;
  struct resolverEntity *left;
};

int arrayTotalIndexes(struct datatype *dtype);
size_t arrayBracketsCalcSize(struct datatype *dtype,
                             struct arrayBrackets *brackets);
size_t arrayBracketsCalculateSizeFromIndex(struct datatype *dtype,
                                           struct arrayBrackets *brackets,
                                           int index);
void arrayBracketsAdd(struct arrayBrackets *brackets, node *bracketNode);
struct arrayBrackets *arrayBracketsNew();
void setLexProcessCompileProcess(struct lexProcess *lpr,
                                 struct compileProcess *process);
void makeBracketNode(node *node);
bool dataTypeIsStructOrUnion(struct datatype *dtype);
scope *scopeNew(compileProcess *process, int flags);
scope *createRootScope(compileProcess *cp);
void scopeFinish(compileProcess *cp);
void parserScopeFinish(compileProcess *currentProcess);
void scopeDealloc(scope *scope);
void scopeFreeRoot(compileProcess *cp);
void makeBodyNode(struct vector *bodyVec, size_t size, bool padded,
                  struct node *largestVarNode);
bool datatypeIsStructOrUnionForName(const char *name);
bool dataTypeIsStructOrUnion(struct datatype *dtype);
size_t datatypeElementSize(struct datatype *dtype);
size_t datatypeSizeForArrayAccess(struct datatype *dtype);
size_t datatypeSizeNoPtr(struct datatype *dtype);
size_t datatypeSize(struct datatype *dtype);
void setLexProcessForCompileProcessForHelpers(struct lexProcess *pr,
                                              struct compileProcess *cp);
int computeSumPadding(struct vector *vec);
int padding(int val, int to);
int alignValue(int val, int to);
int alignValueAsPositive(int val, int to);
bool nodeIsStructOrUnionVariable(struct node *node);
size_t variableSize(struct node *varNode);
node *variableStructOrUnionBodyNode(struct node *node);
void scopePush(compileProcess *cp, void *ptr, size_t elemSize);
void *scopeLastEntityStopAt(compileProcess *cp, scope *stopScope);
struct node *variableNode(struct node *node);
bool datatypeIsPrimitive(struct datatype *dtype);
bool isPrimitive(struct node *node);
void *scopeLastEntity(compileProcess *cp);
node *variableNodeOrList(struct node *node);
void makeStructNode(const char *name, struct node *bodyNode);
void symresolverBuildForNode(compileProcess *process, node *node);
symbol *symresolverGetSymbol(compileProcess *process, const char *name);
struct node *structNodeForName(compileProcess *process, const char *name);
void symResolverInitialize(struct compileProcess *process);

void symresolverNewTable(compileProcess *process);
void symresolverEndTable(compileProcess *process);
bool tokenIsIdentifier(token *token);
void makeFunctionNode(struct datatype *retType, const char *name,
                      struct vector *arguments, struct node *bodyNode);
symbol *symresolverGetSymbolForNativeFunction(compileProcess *process,
                                              const char *name);
bool nodeIsExpressionOrParentheses(struct node *node);
bool nodeIsValueType(struct node *node);
void makeExpParenthesisNode(struct node *expNode);
void makeIfNode(struct node *condNode, struct node *bodyNode,
                struct node *nextNode);
void makeElseNode(struct node *bodyNode);
void makeReturnNode(struct node *expNode);
void makeForNode(struct node *initNode, struct node *condNode,
                 struct node *loopNode, struct node *bodyNode);
void makeWhileNode(struct node *expNode, struct node *bodyNode);
void makeDoWhileNode(struct node *bodyNode, struct node *expNode);
void makeSwitchNode(struct node *expNode, struct node *bodyNode,
                    struct vector *cases, bool hasDefaultCase);
void makeContinueNode();
void makeBreakNode();
void makeLabelNode(struct node *labelNode);
void makeGotoNode(struct node *labelNode);
void makeCaseNode(struct node *labelNode);
void makeTenaryNode(struct node *trueNode, struct node *falseNode);
void makeCastNode(struct datatype *dtype, struct node *opperandNode);
void makeUnionNode(const char *name, struct node *bodyNode);
struct node *unionNodeForName(compileProcess *process, const char *name);
bool isArrayNode(struct node *node);
bool nodeIsExpression(struct node *node, const char *op);
bool isNodeAssignment(struct node *node);
int codegen(struct compileProcess *process);
struct codeGenerator *codegenNew(struct compileProcess *process);
void setCompileProcessForStackFrame(struct compileProcess *cp);
void stackframePush(struct node *funcNode, struct stackFrameElement *element);
void stackframeSub(struct node *funcNode, int type, const char *name,
                   size_t amount);
void stackFramePop(struct node *funcNode);
void stackframeAdd(struct node *funcNode, int type, const char *name,
                   size_t amount);
void stackFrameAssertEmpty(struct node *funcNode);
struct stackFrameElement *stackFrameBackExpect(struct node *funcNode,
                                               int expectingType,
                                               const char *expectingName);
struct stackFrameElement *stackframePeek(struct node *funcNode);
void stackFramePeekStart(struct node *funcNode);
struct stackFrameElement *stackframeBack(struct node *funcNode);
