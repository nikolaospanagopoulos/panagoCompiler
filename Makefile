OBJECTS= ./build/vector.o ./build/compiler.o ./build/compileProcess.o ./build/buffer.o ./build/lexProcess.o ./build/lexer.o ./build/token.o ./build/parser.o ./build/node.o ./build/expressionable.o

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
clean:
	rm ./bin/main
	rm -rf ${OBJECTS}


