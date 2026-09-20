# AST Unparsing (`ast_unparse`)

The `ast_unparse` engine reconstructs formatted source code from Sitting Duck AST tables. It provides a pluggable, rules-based formatter that guarantees the **pseudo-identity property**:

$$\text{parse}(S) \equiv \text{parse}(\text{unparse}(\text{parse}(S)))$$

Parsing the unparsed output of any valid AST yields a syntactically and semantically identical AST.

---

## Functions

### `ast_unparse()`

Reconstructs source code string from an AST table or query expression.

#### Signature

```sql
ast_unparse(ast_table, [indent_size := 4, use_tabs := false, newline := '\n']) -> VARCHAR
```

#### Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `ast_table` | TABLE / QUERY | required | AST relation (from `read_ast()`, `parse_ast()`, or a modified CTE) |
| `indent_size` | INTEGER | `4` | Number of spaces per indentation level |
| `use_tabs` | BOOLEAN | `false` | When `true`, indents using tab characters instead of spaces |
| `newline` | VARCHAR | `'\n'` | Line separator character sequence |

#### Basic Example

```sql
-- Parse Python code into AST, then reconstruct the source code
WITH parsed AS (
    SELECT * FROM parse_ast('def add(a, b): return a + b', 'python')
)
SELECT ast_unparse(parsed) AS reconstructed_code;
```

---

### `ast_unparse_rules()`

Discovers active whitespace, indentation, and linebreak formatting rules for all or a specific language.

#### Signature

```sql
-- All languages (including universal baseline rules)
ast_unparse_rules() -> TABLE

-- Specific language
ast_unparse_rules(language VARCHAR) -> TABLE
```

#### Output Schema

| Column | Type | Description |
|--------|------|-------------|
| `language` | VARCHAR | Target language (`'universal'`, `'python'`, `'cpp'`, `'javascript'`, etc.) |
| `rule_kind` | VARCHAR | Formatting rule kind name (e.g. `SPACE_AFTER`, `BREAK_BEFORE`, `INDENT_CHILDREN`) |
| `node_type` | VARCHAR | AST node type or token target (e.g. `block`, `,`, `:`, `{`, `def`) |
| `int_arg` | BIGINT | Integer parameter (e.g., number of empty lines for `EMPTY_LINES_BEFORE`) |
| `str_arg` | VARCHAR | String parameter (reserved for custom delimiter prefixes/suffixes) |

#### Examples

```sql
-- Inspect Python unparsing rules
SELECT rule_kind, node_type, int_arg
FROM ast_unparse_rules('python');

-- Find all rules specifying linebreaks
SELECT language, rule_kind, node_type
FROM ast_unparse_rules()
WHERE rule_kind LIKE '%BREAK%'
ORDER BY language, node_type;
```

---

## How Unparsing Works

The unparse engine traverses the AST tree using depth-first ordering, respecting hierarchical parent-child relationships and language-specific rules:

1. **Leaf Token Text Extraction:**
   - **Identifiers & Keywords:** Reconstructed from node `name` or punctuation node types.
   - **Verbatim Text Nodes (`NODE_TEXT`):** Leaf nodes such as string fragments, escape sequences, comments, and regex literals reproduce their exact source representations without alteration.

2. **Whitespace and Boundary Control:**
   - Universal baseline rules handle standard punctuation (e.g. trailing space after `,` and `;`, tight bounds inside parentheses `()`, brackets `[]`, and braces `{}`).
   - Language-specific rules configure language idioms (e.g., tight whitespace around `:` in slice expressions vs. trailing space in Python type annotations).

3. **Block Indentation & Nesting:**
   - Container and block constructs (`block`, `statement_block`, `compound_statement`, `class_body`) automatically track lexical nesting depth.
   - `INDENT_CHILDREN` and `INDENT_PARENT_INCREMENT` maintain indentation alignment across multiline structures.

4. **Linebreak & Spacing Policies:**
   - `BREAK_BEFORE` / `BREAK_AFTER` insert newlines at statement and declaration boundaries.
   - `EMPTY_LINES_BEFORE` preserves standard separation between top-level class and function definitions (e.g., 2 blank lines in Python PEP 8).

---

## Formatting Rule Kinds

| Rule Kind | Behavior |
|-----------|----------|
| `SPACE_BEFORE` | Ensures a single whitespace precedes the node |
| `SPACE_AFTER` | Ensures a single whitespace follows the node |
| `NO_SPACE_BEFORE` | Suppresses whitespace before the node (tight on left) |
| `NO_SPACE_AFTER` | Suppresses whitespace after the node (tight on right) |
| `SPACE_AROUND` | Ensures whitespace both before and after the node |
| `TIGHT_INSIDE` | Suppresses inner whitespace for open/close delimiter pairs |
| `BREAK_BEFORE` | Inserts a newline before the node |
| `BREAK_AFTER` | Inserts a newline after the node |
| `EMPTY_LINES_BEFORE` | Inserts N empty lines before the node |
| `BREAK_BETWEEN_CHILDREN` | Inserts a newline between child statements of the node |
| `INDENT_CHILDREN` | Increments indentation level for all child nodes |
| `DEDENT` | Decrements indentation level |

---

## Code Modification & Generation Workflow

Because `ast_unparse()` accepts any table matching Sitting Duck's AST schema, you can manipulate ASTs with standard SQL relational operations (`UPDATE`, `REPLACE`, `CASE`, window functions) and unparse the modified tree:

```sql
-- Rename all occurrences of a function in an AST and unparse back to Python code
WITH original_ast AS (
    SELECT * FROM parse_ast('
def calculate_tax(subtotal):
    rate = 0.08
    return subtotal * rate

total = calculate_tax(100)
', 'python')
),
renamed_ast AS (
    SELECT
        node_id,
        parent_id,
        type,
        CASE
            WHEN name = 'calculate_tax' THEN 'compute_sales_tax'
            ELSE name
        END AS name,
        semantic_type,
        flags,
        file_path,
        language,
        start_line,
        end_line,
        depth,
        sibling_index,
        children_count,
        descendant_count,
        peek
    FROM original_ast
)
SELECT ast_unparse(renamed_ast) AS transformed_code;
```

---

## See Also

- [Core Functions](functions.md) - Main AST parsing and analysis functions
- [Semantic Type System](semantic-types.md) - Universal taxonomy reference
- [Code Transformation Guide](../how-to/code-transformation-and-unparsing.md) - Step-by-step code manipulation tutorial
