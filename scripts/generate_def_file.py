#!/usr/bin/env python3
"""
Script to generate basic .def files from tree-sitter grammars.
This provides a starting point that can be manually refined.
"""

import os
import sys
import json
import re
from pathlib import Path

def analyze_grammar_js(grammar_path):
    """Extract node types from grammar.js file."""
    try:
        with open(grammar_path, 'r') as f:
            content = f.read()
        
        # Extract rule names - these become node types
        # Look for patterns like: rule_name: $ => ...
        rule_pattern = r'(\w+):\s*\$\s*=>'
        rules = re.findall(rule_pattern, content)
        
        # Extract token patterns - these become terminal nodes
        # Look for patterns like: token('keyword')
        token_pattern = r'token\([\'"]([^\'"]+)[\'"]\)'
        tokens = re.findall(token_pattern, content)
        
        # Extract string literals - these become operators/punctuation
        string_pattern = r'[\'"]([^\'"]{1,3})[\'"]'
        strings = re.findall(string_pattern, content)
        
        return {
            'rules': rules,
            'tokens': tokens,
            'strings': [s for s in strings if s in ['(', ')', '[', ']', '{', '}', ',', ';', ':', '+', '-', '*', '/', '=', '==', '!=', '<', '>', '<=', '>=']]
        }
    except Exception as e:
        print(f"Error analyzing grammar: {e}")
        return {'rules': [], 'tokens': [], 'strings': []}

def classify_node_type(node_name):
    """Classify a node type into a normalized category."""
    name_lower = node_name.lower()
    
    # Function-related
    if any(keyword in name_lower for keyword in ['function', 'method', 'procedure']):
        if 'definition' in name_lower or 'declaration' in name_lower:
            return 'FUNCTION_DECLARATION', 'FIND_IDENTIFIER', 'NONE'
        else:
            return 'FUNCTION_CALL', 'NONE', 'NONE'
    
    # Class-related
    if any(keyword in name_lower for keyword in ['class', 'struct', 'interface', 'trait']):
        return 'CLASS_DECLARATION', 'FIND_IDENTIFIER', 'NONE'
    
    # Variable-related
    if any(keyword in name_lower for keyword in ['variable', 'assignment', 'declaration']):
        return 'VARIABLE_DECLARATION', 'FIND_IDENTIFIER', 'NONE'
    
    # Control flow
    if name_lower in ['if_statement', 'if']:
        return 'IF_STATEMENT', 'NONE', 'NONE'
    if any(keyword in name_lower for keyword in ['for', 'while', 'loop']):
        return 'FOR_STATEMENT' if 'for' in name_lower else 'WHILE_STATEMENT', 'NONE', 'NONE'
    if 'return' in name_lower:
        return 'RETURN_STATEMENT', 'NONE', 'NONE'
    if any(keyword in name_lower for keyword in ['break', 'continue']):
        return 'BREAK_STATEMENT' if 'break' in name_lower else 'CONTINUE_STATEMENT', 'NONE', 'NONE'
    
    # Literals
    if any(keyword in name_lower for keyword in ['string', 'number', 'integer', 'float', 'boolean', 'true', 'false', 'null', 'none']):
        if 'string' in name_lower:
            return 'STRING_LITERAL', 'NONE', 'NODE_TEXT'
        elif any(keyword in name_lower for keyword in ['number', 'integer', 'float']):
            return 'NUMBER_LITERAL', 'NONE', 'NODE_TEXT'
        elif any(keyword in name_lower for keyword in ['boolean', 'true', 'false']):
            return 'BOOLEAN_LITERAL', 'NONE', 'NODE_TEXT'
        else:
            return 'NULL_LITERAL', 'NONE', 'NODE_TEXT'
    
    # Expressions
    if 'expression' in name_lower:
        if any(keyword in name_lower for keyword in ['binary', 'comparison', 'logical']):
            return 'BINARY_EXPRESSION', 'NONE', 'NONE'
        elif 'unary' in name_lower:
            return 'UNARY_EXPRESSION', 'NONE', 'NONE'
        elif 'assignment' in name_lower:
            return 'ASSIGNMENT_EXPRESSION', 'NONE', 'NONE'
        elif 'call' in name_lower:
            return 'FUNCTION_CALL', 'NONE', 'NONE'
        else:
            return 'BINARY_EXPRESSION', 'NONE', 'NONE'
    
    # Identifiers
    if 'identifier' in name_lower:
        return 'IDENTIFIER', 'NODE_TEXT', 'NONE'
    
    # Comments
    if 'comment' in name_lower:
        return 'COMMENT', 'NONE', 'NODE_TEXT'
    
    # Imports/exports
    if any(keyword in name_lower for keyword in ['import', 'include']):
        return 'IMPORT_STATEMENT', 'NONE', 'NONE'
    if 'export' in name_lower:
        return 'EXPORT_STATEMENT', 'NONE', 'NONE'
    
    # Blocks
    if any(keyword in name_lower for keyword in ['block', 'compound']):
        return 'BLOCK', 'NONE', 'NONE'
    
    # Module/program
    if any(keyword in name_lower for keyword in ['module', 'program', 'translation_unit']):
        return 'MODULE', 'NONE', 'NONE'
    
    # Default fallback
    return 'IDENTIFIER', 'NONE', 'NONE'

def determine_flags(node_name):
    """Determine flags for a node type."""
    flags = []
    name_lower = node_name.lower()
    
    # Check for literals
    if any(keyword in name_lower for keyword in ['string', 'number', 'integer', 'float', 'boolean', 'true', 'false', 'null', 'none']):
        flags.append('NodeFlags::IS_LITERAL')
    
    # Check for keywords (common language keywords)
    if node_name in ['if', 'else', 'for', 'while', 'do', 'return', 'break', 'continue', 
                     'function', 'class', 'def', 'var', 'let', 'const', 'import', 'export',
                     'public', 'private', 'protected', 'static', 'virtual', 'abstract']:
        flags.append('NodeFlags::IS_KEYWORD')
    
    # Check for punctuation
    if node_name in ['(', ')', '[', ']', '{', '}', ',', ';', ':', '.', '::']:
        flags.append('NodeFlags::IS_PUNCTUATION')
    
    return ' | '.join(flags) if flags else '0'

def generate_def_file(language_name, grammar_data, output_path):
    """Generate a .def file for the given language."""
    
    header = f"""// {language_name.title()} node type mappings
// Format: DEF_TYPE(raw_type, normalized_type, name_extraction, value_extraction, flags)
// Generated by generate_def_file.py - review and refine as needed

"""
    
    entries = []
    
    # Process rules (main node types)
    for rule in sorted(set(grammar_data['rules'])):
        if rule and not rule.startswith('_'):  # Skip private rules
            normalized, name_strat, value_strat = classify_node_type(rule)
            flags = determine_flags(rule)
            entries.append(f'DEF_TYPE({rule}, {normalized}, {name_strat}, {value_strat}, {flags})')
    
    # Process tokens (keywords, etc.)
    for token in sorted(set(grammar_data['tokens'])):
        if token and len(token) > 0:
            normalized, name_strat, value_strat = classify_node_type(token)
            flags = determine_flags(token)
            entries.append(f'DEF_TYPE({token}, {normalized}, {name_strat}, {value_strat}, {flags})')
    
    # Process string literals (operators, punctuation)
    for string in sorted(set(grammar_data['strings'])):
        if string:
            flags = determine_flags(string)
            if 'PUNCTUATION' in flags:
                entries.append(f'DEF_TYPE("{string}", PUNCTUATION, NONE, NONE, {flags})')
            else:
                entries.append(f'DEF_TYPE("{string}", OPERATOR, NONE, NODE_TEXT, {flags})')
    
    content = header + '\n'.join(entries) + '\n'
    
    with open(output_path, 'w') as f:
        f.write(content)
    
    print(f"Generated {output_path} with {len(entries)} entries")

def main():
    if len(sys.argv) != 3:
        print("Usage: python generate_def_file.py <language_name> <grammar_js_path>")
        print("Example: python generate_def_file.py python grammars/tree-sitter-python/grammar.js")
        sys.exit(1)
    
    language_name = sys.argv[1]
    grammar_path = sys.argv[2]
    
    if not os.path.exists(grammar_path):
        print(f"Error: Grammar file not found: {grammar_path}")
        sys.exit(1)
    
    # Analyze the grammar
    print(f"Analyzing {grammar_path}...")
    grammar_data = analyze_grammar_js(grammar_path)
    
    print(f"Found {len(grammar_data['rules'])} rules, {len(grammar_data['tokens'])} tokens, {len(grammar_data['strings'])} strings")
    
    # Generate output path
    output_dir = Path("src/language_configs")
    output_dir.mkdir(exist_ok=True)
    output_path = output_dir / f"{language_name}_types.def"
    
    # Generate the .def file
    generate_def_file(language_name, grammar_data, output_path)
    
    print(f"✅ Generated {output_path}")
    print("⚠️  Please review and refine the generated mappings manually")

if __name__ == "__main__":
    main()