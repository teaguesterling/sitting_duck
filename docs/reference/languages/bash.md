# Bash Node Types

> Bash shell language node type mappings for AST semantic extraction

## Language Characteristics

- **Command-oriented**: Primary unit is command execution, not expressions
- **Pipelines**: Commands chained via `|` for stream processing
- **Expansions**: Variable (`$var`), command (`$()`), arithmetic (`$(())`)
- **Here documents**: Multi-line string input via `<<EOF`
- **Test expressions**: Conditional tests via `[`, `[[`, and `test`
- **Arrays**: Indexed and associative arrays (Bash 4+)
- **Exit codes**: Commands return integer status (0 = success)
- **Subshells**: Isolated execution environments via `()`
- **Background jobs**: Asynchronous execution via `&`
- **Redirections**: I/O stream manipulation (`>`, `<`, `>>`, `2>&1`)

## Semantic Type Encoding

Semantic types use an 8-bit encoding:
- Bits 7-2: Base semantic category (e.g., FLOW_LOOP = 0x24)
- Bits 1-0: Refinement within category (e.g., Loop::ITERATOR = 0x01)

## DEF_TYPE Macro Parameters

```cpp
DEF_TYPE(raw_type, semantic_type, name_extraction, native_extraction, flags)
```

| Parameter | Description |
|-----------|-------------|
| raw_type | Tree-sitter node type string |
| semantic_type | Semantic category with optional refinement |
| name_extraction | Strategy for extracting node name |
| native_extraction | Strategy for rich context extraction |
| flags | Behavioral flags (IS_CONSTRUCT, IS_KEYWORD, etc.) |

## Node Categories

- [Program Structure](#program-structure)
- [Function Definitions](#function-definitions)
- [Variable Declarations and Assignments](#variable-declarations-and-assignments)
- [Commands and Execution](#commands-and-execution)
- [Control Flow](#control-flow)
- [Expansions and Substitutions](#expansions-and-substitutions)
- [String Literals and Quoting](#string-literals-and-quoting)
- [Numbers and Arrays](#numbers-and-arrays)
- [Identifiers and Variables](#identifiers-and-variables)
- [File Operations and Redirections](#file-operations-and-redirections)
- [Comments](#comments)
- [Special Constructs](#special-constructs)
- [Test Expressions and Conditions](#test-expressions-and-conditions)
- [Operators and Punctuation](#operators-and-punctuation)
- [Keywords](#keywords)
- [Error Handling](#error-handling)

## Program Structure

Top-level script structure

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `program` | DEFINITION_MODULE | NONE | Script root - represents the entire Bash script file |

## Function Definitions

Bash function declarations

Bash supports two function syntaxes: - POSIX style: `name() { commands; }` - Bash style: `function name { commands; }` or `function name() { commands; }` Functions in Bash: - Have no formal parameter declarations (use $1, $2, etc.) - Return exit codes (0-255) via `return` or last command status - Can access and modify global variables - Can declare `local` variables for scope isolation

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `function_definition` | DEFINITION_FUNCTION | FIND_IDENTIFIER | Function definition - both POSIX `name()` and Bash `function name` styles |

## Variable Declarations and Assignments

Variable creation and modification

Bash variables: - No explicit type declarations (everything is a string internally) - Assignment uses `name=value` (no spaces around `=`) - `declare`/`typeset` for attributes (-i integer, -a array, -A associative) - `local` for function-scoped variables - `readonly` for immutable variables - `export` for environment variable propagation - `unset` to remove variables

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `variable_assignment` | DEFINITION_VARIABLE | FIND_IDENTIFIER | Variable assignment - `name=value` or `name+=value` |
| `declaration_command` | DEFINITION_VARIABLE | FIND_IDENTIFIER | Declaration command - `declare`, `local`, `readonly`, `typeset`, `export` |
| `unset_command` | DEFINITION_VARIABLE | FIND_IDENTIFIER | Unset command - removes variable or function definition |

## Commands and Execution

Command invocation and pipeline constructs

Command execution in Bash: - Simple commands: `command arg1 arg2` - Pipelines: `cmd1 | cmd2 | cmd3` - Command substitution: `$(command)` or `` `command` `` - Process substitution: `<(command)` or `>(command)` Exit codes: - 0 = success - 1-125 = command-specific errors - 126 = command not executable - 127 = command not found - 128+N = killed by signal N

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `command` | EXECUTION_STATEMENT | FIND_IDENTIFIER | Generic command - wrapper for command structures |
| `simple_command` | EXECUTION_STATEMENT | FIND_IDENTIFIER | Simple command - direct command invocation with arguments |
| `pipeline` | EXECUTION_STATEMENT | NONE | Pipeline - commands connected via `|` for stream processing |
| `command_substitution` | COMPUTATION_CALL | NONE | Command substitution - `$(cmd)` captures command output as string |
| `process_substitution` | COMPUTATION_CALL | NONE | Process substitution - `<(cmd)` or `>(cmd)` provides file descriptor |

## Control Flow

Conditional and loop constructs

Bash control structures: - `if`/`then`/`elif`/`else`/`fi` - conditional branching - `case`/`in`/`esac` - pattern matching (similar to switch) - `for`/`in`/`do`/`done` - iteration over lists - `for ((;;))` - C-style for loop (Bash extension) - `while`/`do`/`done` - condition-based loop - `until`/`do`/`done` - inverse while loop - `break` - exit loop (with optional level N) - `continue` - skip to next iteration - `return` - exit function with status - `exit` - terminate script with status

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `if_statement` | FLOW_CONDITIONAL | NONE | If statement - `if cmd; then ...; fi` |
| `case_statement` | FLOW_CONDITIONAL | NONE | Case statement - pattern matching `case $var in pattern) ...; esac` |
| `while_statement` | FLOW_LOOP | NONE | While loop - `while cmd; do ...; done` |
| `for_statement` | FLOW_LOOP | NONE | For loop - `for var in list; do ...; done` |
| `c_style_for_statement` | FLOW_LOOP | NONE | C-style for loop - `for ((i=0; i<n; i++)); do ...; done` |
| `break_statement` | FLOW_JUMP | NONE | Break statement - exit from enclosing loop |
| `continue_statement` | FLOW_JUMP | NONE | Continue statement - skip to next loop iteration |
| `return_statement` | FLOW_JUMP | NONE | Return statement - exit function with status code |
| `exit_statement` | FLOW_JUMP | NONE | Exit statement - terminate script with status code |
| `else_clause` | FLOW_CONDITIONAL | NONE | Else clause - alternative branch in if statement |
| `elif_clause` | FLOW_CONDITIONAL | NONE | Elif clause - chained conditional in if statement |
| `case_item` | FLOW_CONDITIONAL | NONE | Case item - individual pattern match in case statement |

## Expansions and Substitutions

Variable and expression expansion

Bash expansion order (important for understanding behavior): 1. Brace expansion: `{a,b,c}` → `a b c` 2. Tilde expansion: `~` → home directory 3. Parameter expansion: `$var`, `${var}`, `${var:-default}` 4. Command substitution: `$(cmd)` 5. Arithmetic expansion: `$((expr))` 6. Word splitting: on unquoted results 7. Pathname expansion: glob patterns Parameter expansion modifiers: - `${var:-default}` - use default if unset - `${var:=default}` - assign default if unset - `${var:+alt}` - use alt if set - `${var:?error}` - error if unset - `${#var}` - string length - `${var%pattern}` - remove suffix - `${var#pattern}` - remove prefix

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `expansion` | COMPUTATION_ACCESS | NONE | Complex expansion - `${var}` with modifiers |
| `simple_expansion` | COMPUTATION_ACCESS | NODE_TEXT | Simple expansion - basic `$var` reference |
| `string_expansion` | COMPUTATION_ACCESS | NONE | String expansion - expansion within quoted string |
| `arithmetic_expansion` | COMPUTATION_EXPRESSION | NONE | Arithmetic expansion - `$((expression))` |
| `brace_expression` | COMPUTATION_EXPRESSION | NONE | Brace expression - `{a,b,c}` or `{1..10}` |

## String Literals and Quoting

String representation and quoting mechanisms

Bash quoting mechanisms: - Double quotes (`""`): Allow variable/command expansion - Single quotes (`''`): Literal strings, no expansion - ANSI-C quotes (`$''`): Support escape sequences like `\n`, `\t` - No quotes: Word splitting and glob expansion apply String concatenation is implicit by adjacency: `"Hello "$name` concatenates quoted and expanded parts

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `string` | LITERAL_STRING | NODE_TEXT | Double-quoted string - allows expansion inside |
| `raw_string` | LITERAL_STRING | NODE_TEXT | Single-quoted string - literal, no expansion |
| `ansii_c_string` | LITERAL_STRING | NODE_TEXT | ANSI-C quoted string - `$'...'` with escape sequences |
| `concatenation` | LITERAL_STRING | NODE_TEXT | String concatenation - adjacent strings combined |

## Numbers and Arrays

Numeric and structured literals

Bash numeric handling: - All variables are strings, converted to integers for arithmetic - Arithmetic context: `$((expr))`, `let`, `(( ))` - Base notation: `base#number` (e.g., `2#1010`, `16#FF`) Bash arrays: - Indexed arrays: `arr=(a b c)`, access via `${arr[0]}` - Associative arrays: `declare -A arr; arr[key]=value` (Bash 4+) - All elements: `${arr[@]}` or `${arr[*]}` - Array length: `${#arr[@]}`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `number` | LITERAL_NUMBER | NODE_TEXT | Numeric literal - integers for arithmetic context |
| `array` | LITERAL_STRUCTURED | NONE | Array literal - `(elem1 elem2 elem3)` |

## Identifiers and Variables

Names and variable references

Bash identifier rules: - Start with letter or underscore - Contain letters, digits, underscores - Case-sensitive (`VAR` ≠ `var`) Special parameters: - `$0` - script name - `$1`-`$9`, `${10}` - positional parameters - `$#` - number of parameters - `$@` - all parameters as separate words - `$*` - all parameters as single word - `$?` - last exit status - `$$` - current PID - `$!` - last background PID - `$_` - last argument of previous command

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `variable_name` | NAME_IDENTIFIER | NODE_TEXT | Variable name - identifier in assignment context |
| `word` | NAME_IDENTIFIER | NODE_TEXT | Word - generic token (command name, argument, etc.) |
| `special_variable_name` | NAME_IDENTIFIER | NODE_TEXT | Special variable - `$?`, `$$`, `$!`, `$@`, etc. |

## File Operations and Redirections

I/O redirection and here documents

Redirection operators: - `>` - write stdout to file (truncate) - `>>` - append stdout to file - `<` - read stdin from file - `2>` - redirect stderr - `&>` or `>&` - redirect both stdout and stderr - `2>&1` - redirect stderr to stdout - `<>` - open file for read/write Here documents: - `<<EOF` - multi-line input until delimiter - `<<-EOF` - strip leading tabs - `<<'EOF'` - no variable expansion - `<<<` - here string (single line)

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `file_redirect` | EXTERNAL_IMPORT | NODE_TEXT | File redirect - `>`, `<`, `>>`, `2>`, etc. |
| `heredoc_redirect` | EXTERNAL_IMPORT | NODE_TEXT | Here document - `<<EOF ... EOF` |
| `herestring_redirect` | EXTERNAL_IMPORT | NODE_TEXT | Here string - `<<<string` |

## Comments

Documentation and annotation

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `comment` | METADATA_COMMENT | NODE_TEXT | Comment - starts with `#`, extends to end of line |

## Special Constructs

Block structures and grouping

Bash grouping mechanisms: - `( commands )` - subshell (new process, isolated environment) - `{ commands; }` - command group (same shell, no subshell) - `do ... done` - loop body grouping Subshell characteristics: - Runs in child process - Inherits but cannot modify parent variables - Has its own working directory, traps, options

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `subshell` | ORGANIZATION_BLOCK | NONE | Subshell - commands in `()` run in child process |
| `compound_statement` | ORGANIZATION_BLOCK | NONE | Compound statement - command group in `{}` |
| `do_group` | ORGANIZATION_BLOCK | NONE | Do group - loop body between `do` and `done` |

## Test Expressions and Conditions

Conditional test constructs

Test command forms: - `test expr` - POSIX test command - `[ expr ]` - synonym for test - `[[ expr ]]` - Bash extended test (preferred) Test operators: - String: `-z`, `-n`, `=`, `!=`, `<`, `>` - Numeric: `-eq`, `-ne`, `-lt`, `-le`, `-gt`, `-ge` - File: `-e`, `-f`, `-d`, `-r`, `-w`, `-x`, `-s` - Logic: `!`, `-a`/`&&`, `-o`/`||` `[[ ]]` advantages over `[ ]`: - Pattern matching: `[[ $str == *.txt ]]` - Regex: `[[ $str =~ ^[0-9]+$ ]]` - No word splitting on variables - `&&` and `||` work inside

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `test_command` | COMPUTATION_EXPRESSION | NONE | Test command - `[ ]` or `[[ ]]` conditional expression |
| `binary_expression` | OPERATOR_COMPARISON | NONE | Binary expression - two-operand test (e.g., `$a -eq $b`) |
| `unary_expression` | OPERATOR_LOGICAL | NONE | Unary expression - single-operand test (e.g., `-f file`) |
| `postfix_expression` | OPERATOR_ARITHMETIC | NONE | Postfix expression - increment/decrement in arithmetic |
| `parenthesized_expression` | COMPUTATION_EXPRESSION | NONE | Parenthesized expression - grouped expression `(expr)` |

## Operators and Punctuation

Logical, control, and syntactic operators

Control operators: - `&&` - AND list (run next if previous succeeds) - `||` - OR list (run next if previous fails) - `|` - pipe (connect stdout to stdin) - `&` - background execution - `;` - sequential execution - `;;` - case pattern terminator

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `&&` | OPERATOR_LOGICAL | NODE_TEXT | AND operator - `cmd1 && cmd2` (run cmd2 if cmd1 succeeds) |
| `||` | OPERATOR_LOGICAL | NODE_TEXT | OR operator - `cmd1 || cmd2` (run cmd2 if cmd1 fails) |
| `|` | PARSER_PUNCTUATION | NODE_TEXT | Pipe operator - connect stdout to stdin |
| `&` | OPERATOR_ARITHMETIC | NODE_TEXT | Background operator - run command asynchronously |
| `;` | PARSER_PUNCTUATION | NODE_TEXT | Semicolon - command separator |
| `;;` | PARSER_PUNCTUATION | NODE_TEXT | Double semicolon - case pattern terminator |
| `(` | PARSER_DELIMITER | NODE_TEXT | Left parenthesis - subshell or grouping |
| `)` | PARSER_DELIMITER | NODE_TEXT | Right parenthesis - subshell or grouping |
| `{` | PARSER_DELIMITER | NODE_TEXT | Left brace - command group start |
| `}` | PARSER_DELIMITER | NODE_TEXT | Right brace - command group end |
| `[` | PARSER_DELIMITER | NODE_TEXT | Left bracket - test expression start |
| `]` | PARSER_DELIMITER | NODE_TEXT | Right bracket - test expression end |
| `$` | PARSER_PUNCTUATION | NODE_TEXT | Dollar sign - expansion prefix |

## Keywords

Reserved words with special meaning

Bash reserved words are only recognized in specific contexts: - At the beginning of a command - After `case` or `select` keywords - After `;;` in case statements They can be used as variable names or arguments without quoting in other contexts.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `if` | FLOW_CONDITIONAL | NODE_TEXT | if keyword - begins conditional statement |
| `then` | FLOW_CONDITIONAL | NODE_TEXT | then keyword - begins if/elif body |
| `else` | FLOW_CONDITIONAL | NODE_TEXT | else keyword - begins else body |
| `elif` | FLOW_CONDITIONAL | NODE_TEXT | elif keyword - else-if branch |
| `fi` | FLOW_CONDITIONAL | NODE_TEXT | fi keyword - ends if statement |
| `case` | FLOW_CONDITIONAL | NODE_TEXT | case keyword - begins pattern matching |
| `esac` | FLOW_CONDITIONAL | NODE_TEXT | esac keyword - ends case statement |

## Error Handling

Parser error nodes

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `ERROR` | PARSER_SYNTAX | NODE_TEXT | Error node - represents parse errors |

## Other Node Types

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `for` | FLOW_LOOP | NODE_TEXT | for keyword - begins for loop |
| `while` | FLOW_LOOP | NODE_TEXT | while keyword - begins while loop |
| `until` | FLOW_LOOP | NODE_TEXT | until keyword - begins until loop (inverse while) |
| `do` | FLOW_LOOP | NODE_TEXT | do keyword - begins loop body |
| `done` | FLOW_LOOP | NODE_TEXT | done keyword - ends loop body |
| `in` | FLOW_LOOP | NODE_TEXT | in keyword - list separator in for loop |
| `function` | DEFINITION_FUNCTION | NODE_TEXT | function keyword - optional function declaration prefix |
| `return` | FLOW_JUMP | NODE_TEXT | return keyword - exit function with status |
| `break` | FLOW_JUMP | NODE_TEXT | break keyword - exit loop |
| `continue` | FLOW_JUMP | NODE_TEXT | continue keyword - next loop iteration |
| `exit` | FLOW_JUMP | NODE_TEXT | exit keyword - terminate script |
| `local` | DEFINITION_VARIABLE | NODE_TEXT | local keyword - function-scoped variable |
| `declare` | DEFINITION_VARIABLE | NODE_TEXT | declare keyword - variable declaration with attributes |
| `readonly` | DEFINITION_VARIABLE | NODE_TEXT | readonly keyword - immutable variable declaration |
| `export` | DEFINITION_VARIABLE | NODE_TEXT | export keyword - export variable to environment |

---

*Generated from `bash_types.def`*
