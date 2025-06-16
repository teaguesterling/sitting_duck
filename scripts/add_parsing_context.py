#!/usr/bin/env python3

import os
import re

# Template for GetParsingContext implementation
template = """
LanguageAdapter::ParsingContext {adapter_name}::GetParsingContext() const {{
    ParsingContext ctx;
    ctx.node_configs = &node_configs;
    
    // Capture 'this' to access member functions
    ctx.fallback_name_extractor = [this](TSNode node, const string& content) -> string {{
        // {language}-specific fallback logic
        const char* node_type_str = ts_node_type(node);
        string node_type = string(node_type_str);
        {fallback_logic}
        return "";
    }};
    
    ctx.fallback_value_extractor = [](TSNode node, const string& content) -> string {{
        // No special fallback for values in {language}
        return "";
    }};
    
    ctx.is_public_checker = [this](TSNode node, const string& content) -> bool {{
        {public_logic}
    }};
    
    return ctx;
}}"""

# Language-specific configurations
languages = {
    'javascript': {
        'adapter_name': 'JavaScriptAdapter',
        'fallback_logic': '''if (node_type.find("declaration") != string::npos) {
            return FindChildByType(node, content, "identifier");
        }''',
        'public_logic': '''// In JavaScript, check for export statements or naming conventions
        const char* node_type_str = ts_node_type(node);
        string node_type = string(node_type_str);
        
        // Check if it's an export declaration
        if (node_type.find("export") != string::npos) {
            return true;
        }
        
        // Check parent nodes for export context
        TSNode parent = ts_node_parent(node);
        if (!ts_node_is_null(parent)) {
            const char* parent_type = ts_node_type(parent);
            if (string(parent_type).find("export") != string::npos) {
                return true;
            }
        }
        
        // Check naming conventions - underscore prefix typically indicates private
        string name = ExtractNodeName(node, content);
        if (!name.empty() && name[0] == '_') {
            return false;
        }
        
        // Default to public for JavaScript (no explicit access modifiers)
        return true;'''
    },
    'cpp': {
        'adapter_name': 'CPPAdapter',
        'fallback_logic': '''if (node_type.find("specifier") != string::npos || 
            node_type.find("definition") != string::npos) {
            // Try multiple identifier types
            string result = FindChildByType(node, content, "identifier");
            if (result.empty()) {
                result = FindChildByType(node, content, "type_identifier");
            }
            return result;
        }''',
        'public_logic': '''// For C++, default to public (simplified logic)
        return true;'''
    },
    'typescript': {
        'adapter_name': 'TypeScriptAdapter',
        'fallback_logic': '''if (node_type.find("declaration") != string::npos) {
            return FindChildByType(node, content, "identifier");
        }''',
        'public_logic': '''// Similar to JavaScript with TypeScript access modifiers
        return true;'''
    },
    'sql': {
        'adapter_name': 'SQLAdapter',
        'fallback_logic': '''if (node_type.find("table") != string::npos || node_type.find("view") != string::npos) {
            return FindChildByType(node, content, "identifier");
        }''',
        'public_logic': '''// In SQL, most objects are public
        return true;'''
    },
    'go': {
        'adapter_name': 'GoAdapter',
        'fallback_logic': '''if (node_type.find("declaration") != string::npos) {
            return FindChildByType(node, content, "identifier");
        } else if (node_type.find("_spec") != string::npos) {
            return FindChildByType(node, content, "identifier");
        } else if (node_type == "package_clause") {
            return FindChildByType(node, content, "package_identifier");
        }''',
        'public_logic': '''// In Go, names starting with uppercase are public (exported)
        string name = ExtractNodeName(node, content);
        return !name.empty() && isupper(name[0]);'''
    },
    'ruby': {
        'adapter_name': 'RubyAdapter',
        'fallback_logic': '''if (node_type == "method" || node_type == "singleton_method" || 
            node_type == "class" || node_type == "module") {
            return FindChildByType(node, content, "identifier");
        } else if (node_type == "assignment") {
            return FindChildByType(node, content, "identifier");
        }''',
        'public_logic': '''// In Ruby, default to public unless underscore prefix
        string name = ExtractNodeName(node, content);
        return name.empty() || name[0] != '_';'''
    },
    'markdown': {
        'adapter_name': 'MarkdownAdapter',
        'fallback_logic': '''// No fallback needed for Markdown''',
        'public_logic': '''// In Markdown, all content is public
        return true;'''
    },
    'java': {
        'adapter_name': 'JavaAdapter',
        'fallback_logic': '''if (node_type.find("declaration") != string::npos) {
            return FindChildByType(node, content, "identifier");
        }''',
        'public_logic': '''// In Java, check for explicit access modifiers
        // Simplified - default to false (package visibility)
        return false;'''
    },
    'html': {
        'adapter_name': 'HTMLAdapter',
        'fallback_logic': '''// No fallback needed for HTML''',
        'public_logic': '''// In HTML, all elements are public
        return true;'''
    },
    'css': {
        'adapter_name': 'CSSAdapter',
        'fallback_logic': '''// No fallback needed for CSS''',
        'public_logic': '''// In CSS, all rules are public
        return true;'''
    },
    'c': {
        'adapter_name': 'CAdapter',
        'fallback_logic': '''if (node_type.find("declarator") != string::npos || 
            node_type.find("specifier") != string::npos ||
            node_type.find("definition") != string::npos) {
            return FindChildByType(node, content, "identifier");
        }''',
        'public_logic': '''// In C, default to public (external linkage)
        return true;'''
    }
}

def add_parsing_context(file_path, language, config):
    with open(file_path, 'r') as f:
        content = f.read()
    
    # Check if GetParsingContext already exists
    if 'GetParsingContext() const {' in content:
        print(f"Skipping {file_path} - GetParsingContext already exists")
        return
    
    # Find the end of the file (before namespace closing)
    match = re.search(r'(}\s*//\s*namespace\s+duckdb\s*)$', content)
    if not match:
        print(f"Warning: Could not find namespace closing in {file_path}")
        return
    
    # Insert the GetParsingContext implementation
    insertion_point = match.start()
    implementation = template.format(
        adapter_name=config['adapter_name'],
        language=language.title(),
        fallback_logic=config['fallback_logic'],
        public_logic=config['public_logic']
    )
    
    new_content = content[:insertion_point] + '\n' + implementation + '\n\n' + content[insertion_point:]
    
    with open(file_path, 'w') as f:
        f.write(new_content)
    
    print(f"Added GetParsingContext to {file_path}")

# Process all language adapters
adapters_dir = '/mnt/aux-data/teague/Projects/duckdb_ast/src/language_adapters'
for language, config in languages.items():
    file_path = os.path.join(adapters_dir, f'{language}_adapter.cpp')
    if os.path.exists(file_path):
        add_parsing_context(file_path, language, config)
    else:
        print(f"Warning: {file_path} does not exist")