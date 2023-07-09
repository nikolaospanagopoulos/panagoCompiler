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

enum { INSIDE_EXPRESSION = 0b00000001 };

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
                   struct vector *garbageVec);

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
