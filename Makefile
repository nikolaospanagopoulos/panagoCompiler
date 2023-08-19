OBJECTS= ./build/vector.o ./build/compiler.o ./build/compileProcess.o ./build/buffer.o ./build/lexProcess.o ./build/lexer.o ./build/token.o ./build/parser.o ./build/node.o ./build/expressionable.o ./build/datatype.o ./build/scope.o ./build/symresolver.o ./build/array.o ./build/helpers.o ./build/fixups.o ./build/codegen.o ./build/stackframe.o

all: ${OBJECTS}
	gcc main.c ${OBJECTS}  -g -o ./bin/main
./build/vector.o: ./vector.c
	gcc ./vector.c -o ./build/vector.o -g -c
./build/compiler.o: ./compiler.c
	gcc ./compiler.c -o ./build/compiler.o -g -c
./build/compileProcess.o: ./compileProcess.c
	gcc ./compileProcess.c  -o ./build/compileProcess.o -g -c
./build/buffer.o: ./buffer.c
	gcc ./buffer.c  -o ./build/buffer.o -g -c
./build/lexProcess.o: ./lexProcess.c
	gcc ./lexProcess.c  -o ./build/lexProcess.o -g -c
./build/lexer.o: ./lexer.c
	gcc ./lexer.c  -o ./build/lexer.o -g -c
./build/token.o: ./token.c
	gcc ./token.c -o ./build/token.o -g -c
./build/parser.o: ./parser.c
	gcc ./parser.c -o ./build/parser.o -g -c
./build/node.o: ./node.c
	gcc ./node.c -o ./build/node.o -g -c
./build/expressionable.o: ./expressionable.c
	gcc ./expressionable.c -o ./build/expressionable.o -g -c
./build/datatype.o: ./datatype.c
	gcc ./datatype.c -o ./build/datatype.o -g -c
./build/scope.o: ./scope.c
	gcc ./scope.c -o ./build/scope.o -g -c
./build/symresolver.o: ./symresolver.c
	gcc ./symresolver.c -o ./build/symresolver.o -g -c
./build/array.o: ./array.c
	gcc ./array.c -o ./build/array.o -g -c
./build/helpers.o: ./helpers.c
	gcc ./helpers.c -o ./build/helpers.o -g -c
./build/fixups.o: ./fixup.c
	gcc ./fixup.c -o ./build/fixups.o -g -c
./build/codegen.o: ./codegen.c
	gcc ./codegen.c -o ./build/codegen.o -g -c
./build/stackframe.o: ./stackframe.c
	gcc ./stackframe.c -o ./build/stackframe.o -g -c
clean:
	rm ./bin/main
	rm -rf ${OBJECTS}


