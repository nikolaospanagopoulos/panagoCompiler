# Custom C Compiler

A C compiler developed in C, targeting a subset of the C programming language. This project demonstrates the basics of compiler construction, including parsing, code generation, and assembly.

## Features

- **Subset of C:** Supports essential C language features including variables, functions, conditionals, and loops.
- **Code Generation:** Outputs assembly code for further compilation.
- **Assembly Support:** Utilizes NASM for assembling the generated code.

## Requirements

To build and run this compiler, you will need the following tools installed:

- **GCC** - GNU Compiler Collection for compiling the compiler itself.
- **Make** - Build automation tool.
- **NASM** - Netwide Assembler for assembling the generated assembly code.

## Setup Instructions

Follow these steps to set up and run the compiler on your system:

1. **Clone the Repository:**

   ```bash
   git clone <repository_url>
   ```

2. **Create bin folder:**
   ```bash
   mkdir bin
   ```
3. **make**
   ```bash
   make
   ```
4. **write your C code in ./test.c**
5. **compile**
   ```bash
   ./bin/main
   ```
