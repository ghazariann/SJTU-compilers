# SE3355 Compilers

## Course Overview

In the SE3355 Compilers course, I have developed a complete compiler for the Tiger language over the span of six labs. The course closely follows the book "Modern Compiler Implementation in C" by Andrew Appel, supplemented with specific notes provided in the [docs](/docs) folder.

## Labs Overview

- **Lab 1: Simple Interpreter**: Develop a straightforward interpreter for a straight-line program.
- **Lab 2: Lexical Analysis**: Define the regular language for our compiler in a `.lex` file, focusing on the lexical analysis phase.
- **Lab 3: Parsing**: Develop the context-free grammar in a `.y` file to create an abstract syntax tree.
- **Lab 4: Type Checking**: Perform semantic analysis on the abstract syntax tree, ensuring correct type usage.
- **Lab 5-1: Escape Analysis**: Analyze variable scopes and their lifetimes within the compiler's framework.
- **Lab 5-2: Intermediate Translation & Code Generation**: Translate the high-level abstract syntax tree into an intermediate representation and generate assembly code. This lab results in a working compiler that produces assembly files.
- **Lab 6: Register Allocation & Liveness Analysis**: Implement register allocation using graph coloring algorithms and conduct liveness analysis to optimize resource usage.

## Testing and Documentation

- For each lab, tests are provided along with the source code.
- All tests for all labs have been successfully passed.
- For detailed setup and testing instructions, please refer to [docs/Readme.md](docs/README.md) in this repository.
- For specific requirements of each lab please refer to [docs/labX.pdf](/docs)

## Acknowledgements

A special thanks to the faculty and teaching assistants at SJTU for their guidance and support throughout these courses. Their dedication has been instrumental in providing a comprehensive and practical understanding of compilers.
