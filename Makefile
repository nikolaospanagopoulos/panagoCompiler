OBJECTS= ./build/vector.o ./build/compiler.o ./build/compileProcess.o ./build/buffer.o ./build/lexProcess.o ./build/lexer.o ./build/token.o

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
clean:
	rm ./bin/main
	rm -rf ${OBJECTS}


