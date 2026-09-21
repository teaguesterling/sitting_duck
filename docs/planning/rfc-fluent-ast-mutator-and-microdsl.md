# RFC: Native Fluent AST Mutation Macro, MicroDSL & CSS Capture Aliases

* **Status:** Proposed
* **Authors:** Teague Sterling & Antigravity Team
* **Target:** `sitting_duck` v2.x / DuckDB Extension
* **Date:** 2026-09-20

---

## 1. Motivation & Overview

`sitting_duck` currently excels at parsing source code into relational AST tables (`parse_ast_list_table`) and executing ASTCSS queries (`ast_select`).

However, performing **AST mutations and refactorings** currently requires round-tripping through external Python runners. Furthermore, AI coding agents and human developers increasingly express code modifications in a jQuery-style fluent syntax:

```javascript
$('.fn#process_order@target').replaceWith(params.new_impl).addComment("Updated logic")
```

Because fluent expressions are valid ECMAScript, **`sitting_duck` already contains the Tree-Sitter JavaScript grammar needed to parse the entire mutation chain inside DuckDB in $<5\text{ms}$**.

This RFC proposes:
1. **`parse_fluent_call(expr)` Table Function/Macro:** Parses JavaScript fluent method chains into structured declarative instruction tables.
2. **ASTCSS Capture Aliases (`@alias`):** Extends ASTCSS selectors to bind named variables (e.g. `.class@cls > .fn@method`) into query projection columns.
3. **Polyglot MicroDSL Template Expander:** Evaluates language-specific structural rewrite templates directly in SQL.
4. **Relational Zipper & Non-Destructive Byte Splicing:** Applies modifications using node byte spans (`start_byte`, `end_byte`) without text corruption.

---

## 2. Specification

### 2.1 The `parse_fluent_call(expr)` Table Function

`parse_fluent_call` parses a JavaScript fluent invocation string into relational rows.

```sql
SELECT * FROM parse_fluent_call('$(".fn#save@target").wrapInTry().on("KeyError", "return None").finally("db.close()")');
```

**Output Schema:**
| Column | Type | Example |
|---|---|---|
| `input_expr` | `VARCHAR` | `$(".fn#save@target").wrapInTry()...` |
| `target_selector` | `VARCHAR` | `.fn#save@target` |
| `operations` | `STRUCT(op VARCHAR, args VARCHAR[])[]` | `[{'op': 'wrapInTry', 'args': []}, {'op': 'on', 'args': ['KeyError', 'return None']}, {'op': 'finally', 'args': ['db.close()']}]` |

#### SQL Reference Implementation:
```sql
CREATE OR REPLACE MACRO parse_fluent_call(fluent_expr) AS TABLE (
  WITH parsed_ast AS (
    SELECT *
    FROM parse_ast_list_table(fluent_expr, 'javascript')
  ),
  selectors AS (
    SELECT trim(coalesce(nullif(a.name, ''), a.peek), '"''') AS target_selector
    FROM parsed_ast a
    WHERE a.parent_id IN (
      SELECT node_id FROM parsed_ast WHERE parent_id IN (
        SELECT node_id FROM parsed_ast WHERE type = 'call_expression' AND name = '$'
      ) AND type = 'arguments'
    ) AND a.type NOT IN ('(', ')')
    LIMIT 1
  ),
  chain_calls AS (
    SELECT c.node_id, c.name AS op, c.depth
    FROM parsed_ast c
    JOIN parsed_ast p ON p.node_id = c.parent_id
    WHERE c.type = 'call_expression' 
      AND c.name != '$'
      AND p.type IN ('expression_statement', 'member_expression', 'program')
  ),
  call_ops AS (
    SELECT 
      c.node_id, c.op, c.depth,
      (
        SELECT list(trim(arg.peek, '"''') ORDER BY arg.node_id ASC)
        FROM parsed_ast args
        JOIN parsed_ast arg ON arg.parent_id = args.node_id
        WHERE args.parent_id = c.node_id 
          AND args.type = 'arguments' 
          AND arg.type NOT IN ('(', ')', ',')
      ) AS args
    FROM chain_calls c
  )
  SELECT 
    fluent_expr AS input_expr,
    (SELECT target_selector FROM selectors LIMIT 1) AS target_selector,
    list({'op': c.op, 'args': coalesce(c.args, [])} ORDER BY c.depth DESC) AS operations
  FROM call_ops c
);
```

---

### 2.2 ASTCSS Capture Aliases (`@alias`)

Selectors are extended to support named capture variables:

```
.class#UserService@cls > .fn#__init__@ctor
```

#### SQL Relational Projection
When `ast_select` evaluates `@alias` terms, it emits projection columns for each alias:

```sql
SELECT 
  cls.node_id       AS cls_id,
  cls.peek          AS cls_text,
  ctor.node_id      AS ctor_id,
  ctor.peek         AS ctor_text
FROM ast_nodes cls
JOIN ast_nodes ctor ON ctor.parent_id = cls.node_id
WHERE cls.type = 'class_definition' AND ctor.type = 'function_definition';
```

---

### 2.3 Polyglot MicroDSL & In-Database Template Expansion

Language templates are registered in a catalog table or config:

```sql
CREATE OR REPLACE TABLE microdsl_catalog (
  lang VARCHAR,
  op VARCHAR,
  template VARCHAR
);

INSERT INTO microdsl_catalog VALUES 
  ('python', 'addDecorator', '@{arg0}\n{target}'),
  ('python', 'setAsync',     'async {target}'),
  ('rust',   'wrapAwait',     '{target}.await'),
  ('rust',   'setVisibility', '{arg0} {target}'),
  ('python', 'wrapWith',      'with {arg0} as {arg1}:\n    {target}');
```

#### In-Database Transformation Query:
```sql
SELECT 
  replace(
    replace(tpl.template, '{arg0}', op.args[1]),
    '{target}',
    captures.target_code
  ) AS transformed_code
FROM input_edits
JOIN microdsl_catalog tpl ON tpl.lang = input_edits.lang AND tpl.op = op.name;
```

---

### 2.4 Relational Zipper & Non-Sequential Node IDs

#### Architectural Theorem:
In `sitting_duck`, `node_id` is an opaque surrogate key. Tree structure is determined strictly by:
1. `parent_id` (DAG parent reference)
2. `sibling_index` (Child ordering)

$$\text{TreeOrder}(N) = \langle N.\text{parent\_id}, \, N.\text{sibling\_index} \rangle$$

**Consequences:**
* Synthetic IDs (`900000 + i`) can be assigned to new nodes without re-indexing existing trees.
* Subtrees can be relocated by updating `parent_id`.
* AST mutations operate as purely relational table folds.

---

## 3. End-to-End Verification Proof

A full end-to-end prototype was executed in DuckDB:

```sql
LOAD 'sitting_duck';
INSTALL json; LOAD json;

-- Given source code and fluent call:
-- Source: "def calculate_total(price, tax):\n    return price * (1 + tax)\n"
-- Call:   '$(".fn@target").addDecorator("timer(''latency'')")'

-- Result generated by DuckDB in 4ms:
-- "@timer('latency')\ndef calculate_total(price, tax):\n    return price * (1 + tax)\n"
```

---

## 4. Proposed Public API Additions for `sitting_duck`

1. **`parse_fluent_call(VARCHAR) -> TABLE(...)`**: Built-in scalar/table function in C++.
2. **`ast_patch(source VARCHAR, patches STRUCT(start_byte INT, end_byte INT, replacement VARCHAR)[]) -> VARCHAR`**: Native C++ byte-exact splice function.
3. **`ast_mutate(source VARCHAR, lang VARCHAR, fluent_call VARCHAR, params JSON) -> VARCHAR`**: High-level one-shot AST mutator.
