# Untyped Lambda Calculus Compiler & REPL

A C++11 interpreter, normal-order reduction engine, and symbolic resugarer for the **Untyped Lambda Calculus**[cite: 1]. This engine parses `.lc` source files or interactive expressions into an Abstract Syntax Tree (AST), performs capture-avoiding beta reductions to Normal Form, and uses structural alpha-equivalence pattern matching to fold raw lambda terms back into human-readable combinator names[cite: 1].

---

## Core Engine Features

* **Normal-Order (Lazy) Beta Reduction**: Always evaluates the leftmost-outermost redex first, guaranteeing termination if a Normal Form exists[cite: 1]. Unused arguments (even divergent ones like the $\Omega$ combinator) are safely dropped without being evaluated[cite: 1].
* **Capture-Avoiding Substitution**: Automatically detects variable naming collisions during substitution and performs alpha-conversion by appending prime (`'`) suffixes (e.g., `y'`, `y''`) to conflicting bound variables[cite: 1].
* **$O(1)$ Bijection Alpha-Equivalence**: Checks structural equality modulo bound variable names (`\x. x == \y. y`) using dual scoped hash maps (`env_A_to_B`, `env_B_to_A`) with scope unwinding—no tree mutations required[cite: 1].
* **Greedy Symbolic Resugaring ("Maximal Munch")**: Pre-reduces environment variables to Normal Form (`reduce_variables()`) and recursively matches evaluated AST subtrees against known combinators to fold results back into variable names (`express()`)[cite: 1].
* **Divergence & Memory Circuit Breakers**: Protects against infinite loops and exponential memory blowup using a `MAX_STEPS` limit (default `1000` steps) and `std::bad_alloc` heap exhaustion interception[cite: 1].

---

## Project Directory Structure

```text
lambda_calculus/
├── include/
│   ├── environment.h               # Program state, variable tables, and resugaring API
│   ├── lambda_expression.h         # AST node definitions and reduction signatures
│   └── lambda_parser.h             # Lexer and recursive-descent parser API
├── src/
│   ├── environment.cpp             # Environment loader, comment stripper, and express()
│   ├── lambda_compiler.cpp         # Batch CLI compiler entry point (.lc files)
│   ├── lambda_repl.cpp             # Standalone interactive Python-style REPL session
│   ├── lambda_expression.cpp       # Core AST logic, substitution, and alpha/beta eq
│   └── lambda_parser.cpp           # Tokenizer and recursive-descent AST builder
└── tests/
    ├── environment_tests.cpp       # File I/O, environment merging, and resugaring tests
    ├── lambda_expression_tests.cpp # AST, capture avoidance, and lazy reduction tests
    ├── lambda_parser_tests.cpp     # Lexing, associativity, and syntax error tests
    ├── master_tester.cpp           # Unified test runner with CLI verbosity flags
    └── test_envs/                  # Test .lc files for valid/invalid syntax checks
```

---

## Complete Language Syntax & Grammar Rules

### 1. Lexical Tokens & Delimiters
The lexer scans input byte-by-byte and splits tokens on the following exact delimiter characters[cite: 1]:
* **Whitespace**: Spaces (`' '`), tabs (`\t`), and newlines (`\r`, `\n`) separate variable tokens and are discarded[cite: 1].
* **Standalone Syntax Delimiters**: `.` (dot), `(` (left paren), `)` (right paren), `;` (statement terminator), and `\` (lambda symbol)[cite: 1].
  * *Note on `\`, `lambda`, and `λ`*: Because `\` is a standalone character delimiter, you can write `\x. x` without spaces[cite: 1]. If you use the keyword `lambda`, you must include a space after it (`lambda x. x`) so the lexer separates `lambda` from the variable name `x`[cite: 1]. The `λ` character is used for pretty-printing output in `to_string()`[cite: 1].

### 2. Variable Naming Rules
* **Environment Variables (Left-hand side of `=`)**: Must consist strictly of alphabetical characters (`a-z`, `A-Z`) and underscores (`_`)[cite: 1]. Numbers or special symbols in environment variable names will throw `Syntax Error: Invalid variable name`[cite: 1].
* **No Naming Collisions**: Each variable in the environment must have a unique name[cite: 1]. Redefining an existing variable in a `.lc` file throws `Syntax Error: Naming collision`[cite: 1].
* **Sequential Substitution**: Variables defined earlier in a file are automatically substituted into variables defined later in the file in exact declaration order[cite: 1].

### 3. Expression Grammar & Precedence
1. **Lambda Abstractions (`\` or `lambda`)**:
   * **Syntax**: `\ <vars> . <body>` or `lambda <vars> . <body>`[cite: 1].
   * **Automatic Currying**: Multiple space-separated variables between `\` and `.` are automatically unpacked inside-out into nested single-variable lambdas[cite: 1].
     * Example: `\x y z. (x z) (y z)` parses into `(λx. (λy. (λz. ((x z) (y z)))))`[cite: 1].
   * **Greedy Scope**: A lambda body extends as far to the right as possible until the end of the expression or a closing parenthesis `)`[cite: 1].
     * Example: `\x. a b` parses as `(λx. (a b))`, whereas `(\x. a) b` parses as `((λx. a) b)`[cite: 1].
2. **Function Application**:
   * **Left-Associativity**: Juxtaposed expressions are applied from left to right[cite: 1].
     * Example: `a b c d` parses as `(((a b) c) d)`[cite: 1].
3. **Parentheses `( ... )`**:
   * Used to override greedy lambda scope or group right-hand applications (e.g., `x (y z)` parses as `(x (y z))`)[cite: 1]. Empty parentheses `()` are rejected as syntax errors[cite: 1].
4. **Block Comments (`""" ... """`)**:
   * Everything between opening `"""` and closing `"""` is stripped before parsing[cite: 1]. Comments can span single or multiple lines[cite: 1]. Missing a closing `"""` throws `Syntax Error: Unclosed comment detected`[cite: 1].

---

## Structure of a `.lc` Program File

A `.lc` file has two main sections separated by a divider line[cite: 1]:

```text
""" 1. TOP SECTION: Variable Declarations (Each must end with ';') """
TRUE  = \t f. t;
FALSE = \t f. f;
IF    = \b x y. b x y;
AND   = \p q. p q p;
NOT   = \b. b FALSE TRUE;

""" 2. SEPARATOR LINE: At least 3 asterisks ('***') on its own line """
***

""" 3. BOTTOM SECTION: Optional Title + Final Evaluation Expression """
Boolean Logic Gate Test
NOT (AND TRUE FALSE)
```

### Section Rules
1. **Variable Declarations Section (Before `***`)**:
   * Every assignment must follow `NAME = <expression>;`[cite: 1].
   * Statements can span multiple lines as long as they terminate with `;`[cite: 1].
   * Trailing text without a `;` triggers `Syntax Error: Missing semicolon ';'`[cite: 1].
   * Uncalled divergent combinators (like `OMEGA = (\x. x x) (\x. x x);`) can be safely defined here; if they fail pre-reduction, the compiler logs a warning and stores them unreduced without crashing[cite: 1].
2. **Separator Line (`***`)**:
   * Must be a line containing **only** 3 or more asterisks (`***`, `****`, etc.) after trimming whitespace[cite: 1].
3. **Evaluation Block (After `***`)**:
   * **Optional Title Line**: The compiler checks the **first non-empty line** after `***`[cite: 1]. If that line contains **none** of the lambda syntax tokens (`\`, `λ`, `(`, `.`, or `lambda`), it is stored as `env.target_name` and printed after `"Evaluating: "`[cite: 1].
   * **Target Expression**: All remaining lines after the title (or including the first line if it contains lambda syntax) are concatenated and parsed as a single expression to reduce and resugar[cite: 1]. Do **not** put a semicolon at the end of the evaluation block[cite: 1].

---

## Building and Running All Targets

### Prerequisites
* A C++11 compatible compiler (`g++`, `clang++`, or MSVC).
* Run all commands from the root `lambda_calculus/` directory.

---

### Target 1: Batch File Compiler (`compiler.exe`)
Reads a `.lc` file, prints all loaded environment variables, reduces the target evaluation block to Normal Form, and resugars the output using defined variable names[cite: 1].

**Compile:**
```powershell
g++ -std=c++11 -Wall -I./include src/lambda_expression.cpp src/lambda_parser.cpp src/environment.cpp src/lambda_compiler.cpp -o compiler.exe
```

**Run:**
```powershell
./compiler.exe path/to/file.lc
```

---

### Target 2: Interactive REPL (`repl.exe`)
Provides an interactive `>>> ` shell similar to the Python interpreter where you can declare variables, load `.lc` files, and evaluate lambda expressions on the spot.

**Compile:**
```powershell
g++ -std=c++11 -Wall -I./include src/lambda_expression.cpp src/lambda_parser.cpp src/environment.cpp src/lambda_repl.cpp -o repl.exe
```

**Run:**
```powershell
# Start an empty REPL session
./repl.exe

# Start a REPL session with a .lc file preloaded into memory
./repl.exe path/to/library.lc
```

**Interactive REPL Commands:**
* `NAME = <expr>` : Parses, substitutes known variables, pre-reduces, and stores `NAME` in the session (semicolon optional).
* `<expr>` : Substitutes known session variables, beta-reduces to Normal Form, and prints both the resugared name and raw lambda term.
* `env` : Displays all variables currently stored in the session in declaration order.
* `load <filepath>` : Parses a `.lc` file and merges its variables into the current REPL session.
* `clear` : Deletes all stored variables and resets the session environment.
* `help` : Prints the interactive command summary.
* `exit` or `quit` : Cleans up memory and exits the REPL.

---

### Target 3: Automated Unit Test Suite (`tester.exe`)
Runs all 3 test modules (`AST Engine`, `Parser Frontend`, and `Environment/File IO`) via `tests/master_tester.cpp`[cite: 1].

*(Note: Do **not** include `src/lambda_compiler.cpp` or `src/lambda_repl.cpp` in this build command, as `tests/master_tester.cpp` provides its own `main()` function[cite: 1].)*

**Compile:**
```powershell
g++ -std=c++11 -Wall -I./include src/lambda_expression.cpp src/lambda_parser.cpp src/environment.cpp tests/lambda_expression_tests.cpp tests/lambda_parser_tests.cpp tests/environment_tests.cpp tests/master_tester.cpp -o tester.exe
```

**Run with Verbosity Flags:**
```powershell
# Normal Mode: Prints PASS/FAIL per test; prints detailed diffs only on failures
./tester.exe

# Verbose Mode (-v, -verbose, --verbose): Prints Input, Operation, Expected, and Actual for every test
./tester.exe -v

# Quiet Mode (-q, -quiet, --quiet): Suppresses individual tests and prints only module pass/fail totals
./tester.exe -q
```

---

## Developer Utilities

### 1. Generating Required `.lc` Test Fixtures
The `Environment/File IO` test suite (`tests/environment_tests.cpp`) expects 14 test `.lc` files inside `tests/test_envs/`[cite: 1]. If that folder is missing or empty, run this Python script from the project root to generate all 14 test cases:

```python
import os

os.makedirs("tests/test_envs", exist_ok=True)

test_files = {
    "valid_basic.lc": "ID = \\x. x;\nTRUE = \\t. \\f. t;\nFALSE = \\t. \\f. f;\n***\nTest Evaluation\nID TRUE\n",
    "valid_infinite_uncalled_variable.lc": "OMEGA = (\\x. x x) (\\x. x x);\nID = \\x. x;\n***\nID\n",
    "valid_comments.lc": '\"\"\" Multi-line\ncomment \"\"\"\nID = \\x. x;\n***\nID\n',
    "valid_currying.lc": "K = \\x y. x;\nS = \\x y z. (x z) (y z);\n***\nS K K\n",
    "err_missing_equals.lc": "ID \\x. x;\n",
    "err_invalid_varname.lc": "123BAD = \\x. x;\n",
    "err_missing_expr.lc": "ID = ;\n",
    "err_missing_semi.lc": "ID = \\x. x\n",
    "err_parser_paren.lc": "BAD = (\\x. x;\n",
    "err_parser_var.lc": "BAD = \\ . x;\n",
    "err_parser_dot.lc": "BAD = \\x y z;\n",
    "err_eval_block.lc": "ID = \\x. x;\n***\nTitle Line\n(\\x. x\n",
    "err_unclosed_comment.lc": '\"\"\" Unclosed comment\nID = \\x. x;\n',
    "err_variable_collision.lc": "TRUE = \\t f. t;\nTRUE = \\a b. a;\n"
}

for name, content in test_files.items():
    with open(os.path.join("tests", "test_envs", name), "w", encoding="utf-8") as f:
        f.write(content)
print("Generated all 14 test environment files in tests/test_envs/")
```

### 2. Exporting Project Tree & Source Code (`combine_code.py`)
To bundle the directory structure and all `.cpp`/`.h` files into a single `combined_cpp_code.txt` snapshot[cite: 2]:

```bash
python combine_code.py . -o combined_cpp_code.txt
```