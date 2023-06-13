OBJECTS= ./build/vector.o ./build/compiler.o ./build/compileProcess.o ./build/lexer.o ./build/token.o ./build/position.o

all: ${OBJECTS}
	g++ main.cpp ${OBJECTS}  -g -o ./bin/main
./build/vector.o: ./vector.c
	gcc ./vector.c -o ./build/vector.o -g -c
./build/compiler.o: ./Compiler.cpp
	g++ ./Compiler.cpp -o ./build/compiler.o -g -c
./build/compileProcess.o: ./CompileProcess.cpp
	g++ ./CompileProcess.cpp -o ./build/compileProcess.o -g -c
./build/lexer.o: ./Lexer.cpp
	g++ ./Lexer.cpp -o ./build/lexer.o -g -c
./build/token.o: ./Token.cpp
	g++ ./Token.cpp -o ./build/token.o -g -c
./build/position.o: ./Position.cpp
	g++ ./Position.cpp -o ./build/position.o -g -c
clean:
	rm ./bin/main
	rm -rf ${OBJECTS}


