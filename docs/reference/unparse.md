# AST Unparsing (`ast_unparse`)

The `ast_unparse` suite reconstructs formatted source code from Sitting Duck AST tables. It is a pluggable, rules-based formatter whose **target** property is pseudo-identity:

$$\text{parse}(S) \equiv \text{parse}(\text{unparse}(\text{parse}(S)))$$

That is: re-parsing unparsed output should yield a syntactically and semantically identical AST, even though the *text* may differ in whitespace and formatting.

!!! warning "This is the target, not a guarantee — read the current limits"

    Pseudo-identity holds for most of the 26 tree-sitter languages, but **not all**, and
    the gaps are tracked rather than hidden. As of v1.15.4 (`tracker/features/047`):

    - **Leaf-text coverage landed for 25 of the 26 tree-sitter languages** (PR #199).
      Before that, text-bearing leaves could unparse to their own node type (a comment
      came out as the literal word `comment`).
    - **`sql` is incomplete.** 214 `keyword_*` leaves still lack a text strategy, so
      `ON DELETE NO ACTION` unparses as `ON DELETE keyword_no keyword_action`.
    - **The native `duckdb` language does not unparse at all** ([#197]): its adapter
      reports `children_count = 0` for every node, so the whole tree reads as leaves
      and `SELECT 1` comes back as `program select_statement select_node select_list 1
      unknown_table_ref`. Use the tree-sitter `sql` language instead.
    - **Byte-identity does not hold today**, for any language, even under
      `source := 'full'` — the unparse macros take no `source` parameter and never read
      the position columns. Output differs on inter-token whitespace only. Closing this
      is "4b / node templates" in `047`.

    [#197]: https://github.com/teaguesterling/sitting_duck/issues/197

### How this relates to the planned `write_ast` laws

`docs/planning/v2-architecture.md` is the authoritative statement of where unparse is
going. It splits the requirement by whether the parse retained its source, and the
distinction matters when reading the guarantee above:

| Retention | Law | Strength |
|---|---|---|
| `source := 'full'` | `write_ast(read_ast(x, source := 'full')) = x` | **byte-exact** |
| anything less | `read_ast(write_ast(read_ast(x))) = read_ast(x)` | **pseudo-inverse only** — the tree survives, the text need not |

Below `'full'`, `write_ast(read_ast(x)) = x` is explicitly **not** required: normalising
whitespace there is conformant, not a bug. Today's `ast_unparse` suite implements the
weaker, lower row — byte-exactness is future work, not a current property.

---

## SQL Macros

### `ast_unparse()`

Unparses a source code file on disk into formatted source code text.

```sql
ast_unparse(path VARCHAR, [preset := '']) -> TABLE(file_path VARCHAR, source VARCHAR)
```

#### Example

```sql
SELECT source FROM ast_unparse('src/main.py', 'black');
```

---

### `ast_unparse_code()`

Parses a raw source string into an AST in-memory and unparses it back to formatted source code text.

```sql
ast_unparse_code(source_code VARCHAR, lang VARCHAR, [preset := '']) -> TABLE(file_path VARCHAR, source VARCHAR)
```

#### Example

```sql
SELECT source FROM ast_unparse_code(E'def calc():\n    return 42\n', 'python', 'black');
```

---

### `ast_unparse_from()`

Reconstructs formatted source code from an in-memory AST table, CTE, or view.

```sql
ast_unparse_from(ast_table VARCHAR, [preset := '']) -> TABLE(file_path VARCHAR, source VARCHAR)
```

#### Example

```sql
CREATE TABLE my_ast AS SELECT * FROM parse_ast(E'func add(a int, b int) int {\nreturn a + b\n}', 'go');
SELECT source FROM ast_unparse_from('my_ast', 'gofmt');
```

---

### `ast_unparse_custom()`

Reconstructs formatted source code from an AST table using an arbitrary user-supplied SQL table of formatting rules.

```sql
ast_unparse_custom(ast_table VARCHAR, rules_table VARCHAR) -> TABLE(file_path VARCHAR, source VARCHAR)
```

#### Example

```sql
-- Derive a custom rule set from standard Python rules and override indentation
CREATE TABLE my_rules AS SELECT * FROM ast_unparse_rules('python');
INSERT INTO my_rules VALUES ('python', 'INDENT_STRING', '*', 0, '  ');

SELECT source FROM ast_unparse_custom('my_ast', 'my_rules');
```

---

## Introspection Table Function: `ast_unparse_rules()`

Discovers active whitespace, indentation, and punctuation rules.

### Signatures

```sql
-- All default rules across all built-in languages and universal punctuation
SELECT * FROM ast_unparse_rules();

-- Rules for a specific language
SELECT * FROM ast_unparse_rules(language VARCHAR);

-- Rules for a specific language and style preset
SELECT * FROM ast_unparse_rules(language VARCHAR, preset VARCHAR);
```

### Output Schema

| Column | Type | Description |
|--------|------|-------------|
| `language` | VARCHAR | Language identifier (e.g. `'python'`, `'go'`, `'c'`, or `'*'`) |
| `rule` | VARCHAR | Rule kind (`INDENT_BLOCK`, `INDENT_STRING`, `TIGHT_BEFORE`, `TIGHT_AFTER`, `SPACE_BEFORE`, `SPACE_AFTER`, `LINES_BEFORE`, `LINES_AFTER`, `BREAK_BEFORE`, `BREAK_AFTER`) |
| `target` | VARCHAR | Node type, token literal, or wildcard `'*'` |
| `int_arg` | BIGINT | Numeric argument (e.g. indentation level or line count) |
| `str_arg` | VARCHAR | String argument (e.g. indent characters such as `'    '`, `'  '`, or `'\t'`) |

---

## Formatter Presets Catalog

| Language | Preset Name | Key Characteristics |
| :--- | :--- | :--- |
| **All / Universal** | `default` | Standard language profile defaults |
| **All / Universal** | `tabs` | `INDENT_STRING = '\t'` |
| **All / Universal** | `2spaces` | `INDENT_STRING = '  '` |
| **All / Universal** | `4spaces` | `INDENT_STRING = '    '` |
| **Python** | `pep8` / `black` | 4 spaces, 2 blank lines before top-level class/function definitions |
| **Go** | `gofmt` | `INDENT_STRING = '\t'`, cuddle parens, tight brackets |
| **JavaScript / TypeScript** | `prettier` | 2 spaces indentation (`'  '`) |
| **C / C++** | `llvm` / `google` | 2 spaces indentation (`'  '`) |
| **Rust** | `rustfmt` | 4 spaces indentation (`'    '`) |

---

## Code Modification & Unparsing Workflow

Because `ast_unparse_from()` accepts any table matching Sitting Duck's AST schema, you can manipulate ASTs with standard SQL relational operations (`UPDATE`, `REPLACE`, `CASE`, window functions) and unparse the modified tree:

```sql
-- Rename a function across an AST and unparse back to Python code
CREATE TABLE original_ast AS
SELECT * FROM parse_ast(E'def calculate_tax(subtotal):\n    rate = 0.08\n    return subtotal * rate\n', 'python');

CREATE TABLE modified_ast AS
SELECT
  file_path, node_id, parent_id, type,
  CASE WHEN name = 'calculate_tax' THEN 'compute_sales_tax' ELSE name END AS name,
  semantic_type, flags, language, start_line, end_line, depth,
  sibling_index, children_count, descendant_count, peek
FROM original_ast;

SELECT source FROM ast_unparse_from('modified_ast');
```

---

## See Also

- [Core Functions](functions.md) - Main AST parsing and analysis functions
- [Semantic Type System](semantic-types.md) - Universal taxonomy reference
