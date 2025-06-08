# Function Extraction Flow Using Tree Helpers

## Step-by-Step Visual Guide

### 1. Initial AST Structure

```
class_specifier "Parser" (id=50, desc=200)
├── type_identifier "Parser" (id=51)
├── field_declaration_list (id=52, desc=198)
│   ├── access_specifier "public:" (id=53)
│   ├── function_definition "parse" (id=54, desc=40) ← TARGET
│   │   ├── function_declarator (id=55, desc=5)
│   │   │   ├── qualified_identifier "Parser::parse" (id=56)
│   │   │   └── parameter_list (id=57, desc=3)
│   │   │       ├── parameter_declaration (id=58)
│   │   │       └── parameter_declaration (id=59)
│   │   └── compound_statement (id=60, desc=34)
│   │       ├── if_statement (id=61, desc=10)
│   │       ├── expression_statement (id=72, desc=5)
│   │       └── return_statement (id=78, desc=2)
│   └── function_definition "validate" (id=95, desc=20)
└── ... more content ...
```

### 2. Finding Function Names Without peek

```sql
-- Step 1: Find function declarator nodes
WITH function_nodes AS (
    SELECT * FROM ast WHERE semantic_type = 112  -- DEFINITION_FUNCTION
)
```

Visual Result:
```
[✓] function_declarator (id=55) ← Found
```

### 3. Extract Function Name Using get_children

```sql
-- Step 2: Look at immediate children
SELECT * FROM get_children(ast, 55);
```

Visual Flow:
```
function_declarator (id=55)
├── qualified_identifier "Parser::parse" (id=56) ← NAME HERE!
└── parameter_list (id=57)
    └── ...

Result: function_name = "Parser::parse"
```

### 4. Find Class Context Using get_ancestors

```sql
-- Step 3: Find enclosing class
SELECT * FROM find_nearest_ancestor_of_type(ast, 55, 'class_specifier');
```

Visual Path:
```
class_specifier "Parser" (id=50) ← FOUND!
└── field_declaration_list (id=52)
    └── function_definition (id=54)
        └── function_declarator (id=55) ← Started here
```

### 5. Calculate Complexity Using get_descendants

```sql
-- Step 4: Count control flow in function body
WITH func_body AS (
    SELECT * FROM get_descendants(ast, 54)  -- All 41 nodes
)
SELECT COUNT(*) FROM func_body 
WHERE type IN ('if_statement', 'for_statement', ...);
```

Visual Scope:
```
function_definition (id=54) ← Start
├── [All 40 descendants included in count]
│   ├── ✓ if_statement (complexity +1)
│   ├── ✓ expression_statement
│   └── ✓ return_statement
```

### 6. Complete Extraction Pipeline

```
┌─────────────────┐
│ Find Functions  │ → WHERE semantic_type = 112
└────────┬────────┘
         │
┌────────▼────────┐
│  Get Children   │ → Extract names from identifier nodes
└────────┬────────┘
         │
┌────────▼────────┐
│ Find Ancestors  │ → Determine class context
└────────┬────────┘
         │
┌────────▼────────┐
│ Get Descendants │ → Calculate metrics
└────────┬────────┘
         │
┌────────▼────────┐
│  Final Result   │
└─────────────────┘

Output:
- function_name: "parse"
- class_name: "Parser"  
- qualified_name: "Parser::parse"
- line_count: 25
- complexity: 3
- param_count: 2
```

## Why This Approach is Superior

### ❌ Old Approach (Using peek)
```sql
-- Fragile regex parsing
REGEXP_EXTRACT(peek, 'function\s+(\w+)', 1) as name
-- Problems:
-- - Breaks with templates, namespaces, operators
-- - Language-specific patterns
-- - Can't handle nested structures
```

### ✅ New Approach (Tree Navigation)
```sql
-- Structured tree queries
MAX(CASE WHEN c.type = 'identifier' THEN c.name END)
-- Benefits:
-- - Works across all languages
-- - Handles any syntax correctly  
-- - Follows actual AST structure
-- - No string parsing needed
```

## Common Patterns

### Extract Method Signatures
```
function_declarator
├── identifier/qualified_identifier → Method name
├── parameter_list → Parameter info
│   └── parameter_declaration[] → Individual params
└── trailing_return_type? → Return type
```

### Find Variable Scope
```
1. Start at variable reference
2. Use get_ancestors to find scopes
3. First function/class = containing scope
```

### Analyze Class Hierarchy
```
class_specifier
├── type_identifier → Class name
├── base_class_clause? → Inheritance
└── field_declaration_list
    └── function_definition[] → Methods
```

## Performance Impact

| Query Type | Nodes Scanned | Time Complexity |
|------------|---------------|-----------------|
| Get function name | 2-3 children | O(1) |
| Find class context | Path to root | O(log n) |
| Count complexity | Function subtree | O(k) where k = function size |
| **Total** | **Minimal** | **O(k)** |

Compare to peek parsing:
- Regex on entire peek string: O(m) where m = string length
- Multiple regex patterns: O(m × p) where p = pattern count
- Error prone and language-specific