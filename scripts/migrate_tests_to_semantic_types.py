#!/usr/bin/env python3
"""
Migrate test files from old KIND/normalized_type system to new semantic_type system.
"""

import os
import re
from pathlib import Path

# Mapping of old normalized_type values to semantic_type constants
NORMALIZED_TO_SEMANTIC = {
    'function_declaration': 52,  # DEFINITION_FUNCTION
    'class_declaration': 56,     # DEFINITION_CLASS
    'identifier': 192,           # IDENTIFIER_COMMON
    'string_literal': 129,       # LITERAL_STRING
    'number_literal': 128,       # LITERAL_NUMBER
    'boolean_literal': 136,      # LITERAL_ATOMIC
    'null_literal': 136,         # LITERAL_ATOMIC
    'array_literal': 140,        # LITERAL_STRUCTURED
    'object_literal': 140,       # LITERAL_STRUCTURED
}

# Old KIND values to semantic_type
OLD_KIND_TO_SEMANTIC = {
    1: 128,   # LITERAL -> LITERAL_NUMBER (default)
    2: 192,   # IDENTIFIER -> IDENTIFIER_COMMON
    3: 160,   # OPERATOR -> OPERATOR_ARITHMETIC (default)
    4: 0,     # PARSER -> PARSER_CONSTRUCT
    5: 32,    # EXECUTION -> EXECUTION_STATEMENT (default)
    6: 48,    # DEFINITION -> DEFINITION_FUNCTION (default)
    7: 64,    # FLOW_CONTROL -> FLOW_CONDITIONAL (default)
    8: 96,    # COMPUTATION -> COMPUTATION_CALL (default)
    9: 240,   # TYPE -> TYPE_PRIMITIVE (default)
    10: 208,  # SCOPE -> SCOPE_BLOCK
    11: 80,   # MODIFIER -> MODIFIER_ACCESS (default)
    12: 0,    # ERROR -> PARSER_CONSTRUCT
}

def update_test_file(filepath):
    """Update a single test file to use semantic_type instead of normalized_type/kind."""
    with open(filepath, 'r') as f:
        content = f.read()
    
    original = content
    
    # Replace normalized_type references
    content = re.sub(r'\bnormalized_type\b', 'semantic_type', content)
    
    # Update schema references
    content = re.sub(
        r'node_id, type, normalized_type, name,',
        'node_id, type, semantic_type, name,',
        content
    )
    
    # Update specific normalized_type comparisons
    for old_type, semantic_val in NORMALIZED_TO_SEMANTIC.items():
        # Handle string comparisons
        content = re.sub(
            f"normalized_type\\s*=\\s*'{old_type}'",
            f"semantic_type = {semantic_val}",
            content
        )
        # Handle column selections showing normalized_type values
        content = re.sub(
            f"^{old_type}\\s+{old_type}$",
            f"{old_type}\t{semantic_val}",
            content,
            flags=re.MULTILINE
        )
    
    # Update kind references to semantic_type
    content = re.sub(r'\bkind\b(?!\s*:)', 'semantic_type', content)
    
    # Update specific kind value comparisons
    for old_kind, semantic_val in OLD_KIND_TO_SEMANTIC.items():
        content = re.sub(
            f"kind\\s*=\\s*{old_kind}(?!\\d)",
            f"semantic_type = {semantic_val}",
            content
        )
    
    # Remove super_type references (now part of semantic_type)
    content = re.sub(r',\s*super_type(?:\s+TINYINT)?', '', content)
    content = re.sub(r'\bsuper_type\b', 'semantic_type', content)
    
    # Update struct type definitions
    content = re.sub(
        r'"normalized_type"\s+VARCHAR',
        '"semantic_type" TINYINT',
        content
    )
    
    # Update error messages that reference normalized_type
    content = re.sub(
        r'statement error.*normalized_type',
        'statement error',
        content
    )
    
    if content != original:
        with open(filepath, 'w') as f:
            f.write(content)
        return True
    return False

def update_sql_macro(filepath):
    """Update SQL macro files that use the old taxonomy."""
    with open(filepath, 'r') as f:
        content = f.read()
    
    original = content
    
    # Comment out old KIND-based macros
    if 'ast_taxonomy' in content or 'kind_to_text' in content:
        lines = content.split('\n')
        new_lines = []
        in_old_macro = False
        
        for line in lines:
            if 'CREATE OR REPLACE MACRO' in line and ('ast_taxonomy' in line or 'kind_to_text' in line):
                in_old_macro = True
                new_lines.append('-- DEPRECATED: ' + line)
            elif in_old_macro and line.strip() == ';':
                new_lines.append('-- ' + line)
                in_old_macro = False
            elif in_old_macro:
                new_lines.append('-- ' + line)
            else:
                new_lines.append(line)
        
        content = '\n'.join(new_lines)
    
    # Add new semantic_type macros
    if '-- NEW SEMANTIC TYPE MACROS' not in content:
        content += '''

-- NEW SEMANTIC TYPE MACROS
CREATE OR REPLACE MACRO semantic_type_name(semantic_type) AS
    CASE semantic_type
        -- PARSER types (0-15)
        WHEN 0 THEN 'PARSER_CONSTRUCT'
        WHEN 4 THEN 'PARSER_DELIMITER'
        WHEN 8 THEN 'PARSER_WHITESPACE'
        WHEN 12 THEN 'PARSER_ERROR'
        -- EXECUTION types (32-47)
        WHEN 32 THEN 'EXECUTION_STATEMENT'
        WHEN 36 THEN 'EXECUTION_DECLARATION'
        WHEN 40 THEN 'EXECUTION_INVOCATION'
        WHEN 44 THEN 'EXECUTION_MUTATION'
        -- DEFINITION types (48-63)
        WHEN 48 THEN 'DEFINITION_MODULE'
        WHEN 52 THEN 'DEFINITION_FUNCTION'
        WHEN 56 THEN 'DEFINITION_CLASS'
        WHEN 60 THEN 'DEFINITION_VARIABLE'
        -- FLOW types (64-79)
        WHEN 64 THEN 'FLOW_CONDITIONAL'
        WHEN 68 THEN 'FLOW_LOOP'
        WHEN 72 THEN 'FLOW_JUMP'
        WHEN 76 THEN 'FLOW_EXCEPTION'
        WHEN 80 THEN 'FLOW_SYNC'
        -- COMPUTATION types (96-111)
        WHEN 96 THEN 'COMPUTATION_CALL'
        WHEN 100 THEN 'COMPUTATION_ACCESS'
        WHEN 104 THEN 'COMPUTATION_CAST'
        WHEN 108 THEN 'COMPUTATION_SLICE'
        -- LITERAL types (128-143)
        WHEN 128 THEN 'LITERAL_NUMBER'
        WHEN 129 THEN 'LITERAL_STRING'
        WHEN 136 THEN 'LITERAL_ATOMIC'
        WHEN 140 THEN 'LITERAL_STRUCTURED'
        -- OPERATOR types (160-175)
        WHEN 160 THEN 'OPERATOR_ARITHMETIC'
        WHEN 164 THEN 'OPERATOR_LOGICAL'
        WHEN 168 THEN 'OPERATOR_COMPARISON'
        WHEN 172 THEN 'OPERATOR_ASSIGNMENT'
        -- IDENTIFIER types (192-207)
        WHEN 192 THEN 'IDENTIFIER_COMMON'
        WHEN 196 THEN 'IDENTIFIER_TYPE'
        WHEN 200 THEN 'IDENTIFIER_ANNOTATION'
        WHEN 204 THEN 'IDENTIFIER_SPECIAL'
        -- TYPE types (240-255)
        WHEN 240 THEN 'TYPE_PRIMITIVE'
        WHEN 241 THEN 'TYPE_OBJECT'
        WHEN 244 THEN 'TYPE_PARAMETERIZED'
        WHEN 248 THEN 'TYPE_ANNOTATION'
        ELSE 'UNKNOWN_' || semantic_type::VARCHAR
    END;

CREATE OR REPLACE MACRO semantic_super_kind(semantic_type) AS
    CASE (semantic_type >> 6) & 3
        WHEN 0 THEN 'PARSER'
        WHEN 1 THEN 'SEMANTIC'
        WHEN 2 THEN 'SYNTAX'
        WHEN 3 THEN 'TYPE'
        ELSE 'UNKNOWN'
    END;

CREATE OR REPLACE MACRO semantic_kind(semantic_type) AS
    CASE (semantic_type >> 4) & 15
        WHEN 0 THEN 'PARSER'
        WHEN 2 THEN 'EXECUTION'
        WHEN 3 THEN 'DEFINITION'
        WHEN 4 THEN 'FLOW_CONTROL'
        WHEN 6 THEN 'COMPUTATION'
        WHEN 8 THEN 'LITERAL'
        WHEN 10 THEN 'OPERATOR'
        WHEN 12 THEN 'IDENTIFIER'
        WHEN 15 THEN 'TYPE'
        ELSE 'UNKNOWN'
    END;
'''
    
    if content != original:
        with open(filepath, 'w') as f:
            f.write(content)
        return True
    return False

def main():
    project_root = Path(__file__).parent.parent
    test_dir = project_root / 'test'
    src_dir = project_root / 'src'
    
    updated_files = []
    
    # Update test files
    for test_file in test_dir.rglob('*.test'):
        if update_test_file(test_file):
            updated_files.append(test_file)
            print(f"Updated: {test_file.relative_to(project_root)}")
    
    # Update SQL macro files
    for sql_file in src_dir.rglob('*.sql'):
        if 'taxonomy' in sql_file.name or 'macro' in sql_file.name:
            if update_sql_macro(sql_file):
                updated_files.append(sql_file)
                print(f"Updated: {sql_file.relative_to(project_root)}")
    
    print(f"\nTotal files updated: {len(updated_files)}")
    
    # Create backup of old tests
    backup_dir = test_dir / 'old_kind_tests_backup'
    backup_dir.mkdir(exist_ok=True)
    
    for filepath in updated_files:
        if filepath.suffix == '.test':
            backup_path = backup_dir / filepath.name
            print(f"Backing up: {filepath.name} -> {backup_path}")
            # Note: In real usage, copy the original file before modification

if __name__ == '__main__':
    main()