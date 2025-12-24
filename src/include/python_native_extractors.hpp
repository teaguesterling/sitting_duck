#pragma once

#include "native_context_extraction.hpp"
#include "function_call_extractor.hpp"
#include <tree_sitter/api.h>

namespace duckdb {

//==============================================================================
// Python-Specific Native Context Extractors
//==============================================================================

// Base template for Python extractors - default returns empty context
template<NativeExtractionStrategy Strategy>
struct PythonNativeExtractor {
    static NativeContext Extract(TSNode node, const string& content) {
        return NativeContext(); // Default: no extraction
    }
};

// Specialization for FUNCTION_WITH_PARAMS
template<>
struct PythonNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;
        
        // Extract return type if present (for type-annotated functions)  
        string return_type = ExtractPythonReturnType(node, content);
        if (!return_type.empty()) {
            context.signature_type = return_type;
        }
        // Note: For Python, many functions don't have type annotations, so empty signature_type is normal
        
        // Extract function parameters
        context.parameters = ExtractPythonParameters(node, content);
        
        // Extract decorators if present
        auto decorators = ExtractPythonDecorators(node, content);
        context.modifiers = decorators;
        
        return context;
    }
    
private:
    static string ExtractPythonReturnType(TSNode node, const string& content) {
        // Look for type annotation after -> in function definition
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "->") == 0 && i + 1 < child_count) {
                // Next child should be the return type
                TSNode type_node = ts_node_child(node, i + 1);
                uint32_t start = ts_node_start_byte(type_node);
                uint32_t end = ts_node_end_byte(type_node);
                if (start < content.length() && end <= content.length()) {
                    return content.substr(start, end - start);
                }
            }
        }
        return ""; // No return type annotation
    }

    static vector<ParameterInfo> ExtractPythonParameters(TSNode node, const string& content) {
        vector<ParameterInfo> params;
        
        // Find parameters node
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "parameters") == 0) {
                // Extract each parameter from the parameters list
                params = ExtractPythonParametersDirect(child, content);
                break;
            }
        }
        
        return params;
    }
    
    static vector<ParameterInfo> ExtractPythonParametersDirect(TSNode params_node, const string& content) {
        vector<ParameterInfo> parameters;
        
        uint32_t child_count = ts_node_child_count(params_node);
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(params_node, i);
            const char* child_type = ts_node_type(child);
            
            ParameterInfo param;
            bool is_valid_param = false;
            
            if (strcmp(child_type, "identifier") == 0) {
                // Simple parameter: def func(param):
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    param.name = content.substr(start, end - start);
                    is_valid_param = true;
                }
            } else if (strcmp(child_type, "typed_parameter") == 0) {
                // Typed parameter: def func(param: int):
                param = ExtractTypedParameter(child, content);
                is_valid_param = !param.name.empty();
            } else if (strcmp(child_type, "default_parameter") == 0) {
                // Parameter with default: def func(param=default):
                param = ExtractDefaultParameter(child, content);
                is_valid_param = !param.name.empty();
            } else if (strcmp(child_type, "typed_default_parameter") == 0) {
                // Typed parameter with default: def func(param: int = default):
                param = ExtractTypedDefaultParameter(child, content);
                is_valid_param = !param.name.empty();
            }
            
            if (is_valid_param) {
                parameters.push_back(param);
            }
        }
        
        return parameters;
    }
    
    static ParameterInfo ExtractTypedParameter(TSNode node, const string& content) {
        ParameterInfo param;
        uint32_t child_count = ts_node_child_count(node);
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "identifier") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    param.name = content.substr(start, end - start);
                }
            } else if (strcmp(child_type, "type") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    param.type = content.substr(start, end - start);
                }
            }
        }
        
        return param;
    }
    
    static ParameterInfo ExtractDefaultParameter(TSNode node, const string& content) {
        ParameterInfo param;
        param.is_optional = true;
        uint32_t child_count = ts_node_child_count(node);
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "identifier") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    param.name = content.substr(start, end - start);
                }
            } else if (strcmp(child_type, "=") != 0 && !param.name.empty()) {
                // This is likely the default value (skip the = sign)
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    param.default_value = content.substr(start, end - start);
                }
            }
        }
        
        return param;
    }
    
    static ParameterInfo ExtractTypedDefaultParameter(TSNode node, const string& content) {
        ParameterInfo param;
        param.is_optional = true;
        uint32_t child_count = ts_node_child_count(node);
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "identifier") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    param.name = content.substr(start, end - start);
                }
            } else if (strcmp(child_type, "type") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    param.type = content.substr(start, end - start);
                }
            } else if (strcmp(child_type, "=") != 0 && 
                       strcmp(child_type, "identifier") != 0 &&
                       strcmp(child_type, "type") != 0 &&
                       strcmp(child_type, ":") != 0) {
                // This is likely the default value
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    param.default_value = content.substr(start, end - start);
                }
            }
        }
        
        return param;
    }
    
    static vector<string> ExtractPythonDecorators(TSNode node, const string& content) {
        vector<string> decorators;
        
        // Check if this function has decorators (they appear as siblings before the function)
        TSNode parent = ts_node_parent(node);
        if (!ts_node_is_null(parent)) {
            uint32_t child_count = ts_node_child_count(parent);
            for (uint32_t i = 0; i < child_count; i++) {
                TSNode child = ts_node_child(parent, i);
                const char* child_type = ts_node_type(child);
                
                if (strcmp(child_type, "decorator") == 0) {
                    // Extract decorator name
                    uint32_t start = ts_node_start_byte(child);
                    uint32_t end = ts_node_end_byte(child);
                    if (start < content.length() && end <= content.length()) {
                        string decorator_text = content.substr(start, end - start);
                        decorators.push_back(decorator_text);
                    }
                }
            }
        }
        
        return decorators;
    }
};

// Specialization for ASYNC_FUNCTION
template<>
struct PythonNativeExtractor<NativeExtractionStrategy::ASYNC_FUNCTION> {
    static NativeContext Extract(TSNode node, const string& content) {
        // Reuse FUNCTION_WITH_PARAMS logic and add async modifier
        auto context = PythonNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS>::Extract(node, content);
        context.modifiers.insert(context.modifiers.begin(), "async");
        return context;
    }
};

// Specialization for CLASS_WITH_METHODS
template<>
struct PythonNativeExtractor<NativeExtractionStrategy::CLASS_WITH_METHODS> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;

        try {
            string node_type = ts_node_type(node);

            if (node_type == "class_definition") {
                context.signature_type = ExtractClassType(node, content);
                context.parameters = ExtractBaseClassesAsParameters(node, content);
                context.modifiers = ExtractPythonClassModifiers(node, content, !context.parameters.empty());
            } else if (node_type == "decorated_definition") {
                context.signature_type = ExtractDecoratedClassType(node, content);
                context.parameters = ExtractDecoratedBaseClassesAsParameters(node, content);
                context.modifiers = ExtractDecoratedClassModifiers(node, content, !context.parameters.empty());
            } else {
                // Default class extraction
                context.signature_type = "class";
                vector<string> base_classes = ExtractPythonBaseClasses(node, content);
                for (const string& base : base_classes) {
                    context.parameters.push_back({base, "extends"});
                }
            }
        } catch (...) {
            context.signature_type = "class";
            context.modifiers.clear();
            context.parameters.clear();
        }

        return context;
    }
    
private:
    static string ExtractClassType(TSNode node, const string& content) {
        // Check for special class types based on base classes and content
        vector<string> base_classes = ExtractPythonBaseClasses(node, content);
        
        // Check for common patterns
        for (const string& base : base_classes) {
            if (base == "ABC" || base == "AbstractBase") {
                return "abstract_class";
            } else if (base == "Enum") {
                return "enum_class";
            } else if (base == "IntEnum") {
                return "int_enum_class";
            } else if (base == "Exception" || base.find("Exception") != string::npos) {
                return "exception_class";
            } else if (base == "type") {
                return "metaclass";
            }
        }
        
        // Check for class body to detect special patterns
        if (HasAbstractMethods(node, content)) {
            return "abstract_class";
        }
        
        if (HasDataclassDecorator(node, content)) {
            return "dataclass";
        }
        
        return "class";
    }
    
    static string ExtractDecoratedClassType(TSNode node, const string& content) {
        // For decorated classes, extract the class type from the inner definition
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "class_definition") == 0) {
                string base_type = ExtractClassType(child, content);
                
                // Check decorators for type modification
                vector<string> decorators = ExtractClassDecorators(node, content);
                for (const string& decorator : decorators) {
                    if (decorator.find("@dataclass") != string::npos) {
                        return "dataclass";
                    } else if (decorator.find("@attr.s") != string::npos || decorator.find("@attrs") != string::npos) {
                        return "attrs_class";
                    } else if (decorator.find("@final") != string::npos) {
                        return "final_class";
                    }
                }
                
                return "decorated_" + base_type;
            }
        }
        
        return "decorated_class";
    }
    
    // Extract base classes as ParameterInfo objects (parent classes go in parameters)
    // Python doesn't distinguish extends/implements, so all parents are "extends"
    static vector<ParameterInfo> ExtractBaseClassesAsParameters(TSNode node, const string& content) {
        vector<ParameterInfo> params;
        vector<string> base_classes = ExtractPythonBaseClasses(node, content);
        for (const string& base : base_classes) {
            params.push_back({base, "extends"});  // name = class name, type = inheritance kind
        }
        return params;
    }

    // Extract base classes from decorated class definition
    static vector<ParameterInfo> ExtractDecoratedBaseClassesAsParameters(TSNode node, const string& content) {
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            if (strcmp(ts_node_type(child), "class_definition") == 0) {
                return ExtractBaseClassesAsParameters(child, content);
            }
        }
        return {};
    }

    // Extract class modifiers (abstract, has_classmethods, etc.)
    // Note: inheritance info is now in ParameterInfo.type, not in modifiers
    static vector<string> ExtractPythonClassModifiers(TSNode node, const string& content, bool has_parents = false) {
        vector<string> modifiers;

        // Check for special method patterns
        if (HasAbstractMethods(node, content)) {
            modifiers.push_back("abstract");
        }

        if (HasClassMethods(node, content)) {
            modifiers.push_back("has_classmethods");
        }

        if (HasStaticMethods(node, content)) {
            modifiers.push_back("has_staticmethods");
        }

        if (HasProperties(node, content)) {
            modifiers.push_back("has_properties");
        }

        if (HasDunderMethods(node, content)) {
            modifiers.push_back("has_dunder_methods");
        }

        return modifiers;
    }

    static vector<string> ExtractDecoratedClassModifiers(TSNode node, const string& content, bool has_parents = false) {
        vector<string> modifiers;

        // Extract decorators
        vector<string> decorators = ExtractClassDecorators(node, content);
        for (const string& decorator : decorators) {
            modifiers.push_back(decorator);
        }

        // Get base class modifiers from the inner class definition
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);

            if (strcmp(child_type, "class_definition") == 0) {
                vector<string> class_modifiers = ExtractPythonClassModifiers(child, content, has_parents);
                modifiers.insert(modifiers.end(), class_modifiers.begin(), class_modifiers.end());
                break;
            }
        }

        return modifiers;
    }
    
    static vector<string> ExtractPythonBaseClasses(TSNode node, const string& content) {
        vector<string> base_classes;
        
        // Find argument_list node which contains base classes
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "argument_list") == 0) {
                // Extract base class names
                uint32_t arg_count = ts_node_child_count(child);
                for (uint32_t j = 0; j < arg_count; j++) {
                    TSNode arg = ts_node_child(child, j);
                    string arg_type = ts_node_type(arg);
                    
                    if (arg_type == "identifier" || arg_type == "attribute") {
                        uint32_t start = ts_node_start_byte(arg);
                        uint32_t end = ts_node_end_byte(arg);
                        if (start < content.length() && end <= content.length()) {
                            string base_class = content.substr(start, end - start);
                            base_classes.push_back(base_class);
                        }
                    }
                }
                break;
            }
        }
        
        return base_classes;
    }
    
    static vector<string> ExtractClassDecorators(TSNode node, const string& content) {
        vector<string> decorators;
        
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "decorator") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    string decorator_text = content.substr(start, end - start);
                    decorators.push_back(decorator_text);
                }
            }
        }
        
        return decorators;
    }
    
    static bool HasAbstractMethods(TSNode node, const string& content) {
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "block") == 0) {
                // Check for @abstractmethod decorator in the block
                uint32_t block_count = ts_node_child_count(child);
                for (uint32_t j = 0; j < block_count; j++) {
                    TSNode block_child = ts_node_child(child, j);
                    const char* block_child_type = ts_node_type(block_child);
                    
                    if (strcmp(block_child_type, "decorated_definition") == 0) {
                        uint32_t start = ts_node_start_byte(block_child);
                        uint32_t end = ts_node_end_byte(block_child);
                        if (start < content.length() && end <= content.length()) {
                            string method_text = content.substr(start, end - start);
                            if (method_text.find("@abstractmethod") != string::npos) {
                                return true;
                            }
                        }
                    }
                }
            }
        }
        
        return false;
    }
    
    static bool HasClassMethods(TSNode node, const string& content) {
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "block") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    string class_text = content.substr(start, end - start);
                    if (class_text.find("@classmethod") != string::npos) {
                        return true;
                    }
                }
            }
        }
        
        return false;
    }
    
    static bool HasStaticMethods(TSNode node, const string& content) {
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "block") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    string class_text = content.substr(start, end - start);
                    if (class_text.find("@staticmethod") != string::npos) {
                        return true;
                    }
                }
            }
        }
        
        return false;
    }
    
    static bool HasProperties(TSNode node, const string& content) {
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "block") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    string class_text = content.substr(start, end - start);
                    if (class_text.find("@property") != string::npos) {
                        return true;
                    }
                }
            }
        }
        
        return false;
    }
    
    static bool HasDunderMethods(TSNode node, const string& content) {
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "block") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    string class_text = content.substr(start, end - start);
                    if (class_text.find("def __") != string::npos && class_text.find("__(") != string::npos) {
                        return true;
                    }
                }
            }
        }
        
        return false;
    }
    
    static bool HasDataclassDecorator(TSNode node, const string& content) {
        // Check if the class has a @dataclass decorator
        TSNode parent = ts_node_parent(node);
        if (!ts_node_is_null(parent)) {
            string parent_type = ts_node_type(parent);
            if (parent_type == "decorated_definition") {
                uint32_t start = ts_node_start_byte(parent);
                uint32_t end = ts_node_end_byte(parent);
                if (start < content.length() && end <= content.length()) {
                    string decorated_text = content.substr(start, end - start);
                    if (decorated_text.find("@dataclass") != string::npos) {
                        return true;
                    }
                }
            }
        }
        
        return false;
    }
};

// Specialization for VARIABLE_WITH_TYPE
template<>
struct PythonNativeExtractor<NativeExtractionStrategy::VARIABLE_WITH_TYPE> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;
        
        try {
            string node_type = ts_node_type(node);
            
            if (node_type == "typed_parameter") {
                context.signature_type = ExtractTypedParameterType(node, content);
                context.modifiers = ExtractTypedParameterModifiers(node, content);
            } else if (node_type == "type") {
                context.signature_type = ExtractTypeText(node, content);
                context.modifiers = ExtractTypeModifiers(node, content);
            } else if (node_type == "identifier") {
                context.signature_type = ExtractIdentifierType(node, content);
                context.modifiers = ExtractIdentifierModifiers(node, content);
            } else if (node_type == "dotted_name") {
                context.signature_type = ExtractDottedNameType(node, content);
                context.modifiers = ExtractDottedNameModifiers(node, content);
            } else if (node_type == "assignment") {
                context.signature_type = ExtractAssignmentType(node, content);
                context.modifiers = ExtractAssignmentModifiers(node, content);
            } else if (node_type == "annotated_assignment") {
                context.signature_type = ExtractAnnotatedAssignmentType(node, content);
                context.modifiers = ExtractAnnotatedAssignmentModifiers(node, content);
            } else if (node_type == "attribute") {
                context.signature_type = ExtractAttributeType(node, content);
                context.modifiers = ExtractAttributeModifiers(node, content);
            } else if (node_type == "subscript") {
                context.signature_type = ExtractSubscriptType(node, content);
                context.modifiers = ExtractSubscriptModifiers(node, content);
            } else if (node_type == "list_comprehension" || node_type == "set_comprehension" || 
                      node_type == "dictionary_comprehension" || node_type == "generator_expression") {
                context.signature_type = ExtractComprehensionType(node, content);
                context.modifiers = ExtractComprehensionModifiers(node, content);
            } else {
                // For other types, try to extract type annotation if present
                uint32_t child_count = ts_node_child_count(node);
                for (uint32_t i = 0; i < child_count; i++) {
                    TSNode child = ts_node_child(node, i);
                    const char* child_type = ts_node_type(child);
                    
                    if (strcmp(child_type, "type") == 0) {
                        uint32_t start = ts_node_start_byte(child);
                        uint32_t end = ts_node_end_byte(child);
                        if (start < content.length() && end <= content.length()) {
                            context.signature_type = content.substr(start, end - start);
                        }
                        break;
                    }
                }
            }
        } catch (...) {
            context.signature_type = "";
            context.modifiers.clear();
        }
        
        return context;
    }
    
private:
    static string ExtractTypedParameterType(TSNode node, const string& content) {
        // Extract type from typed parameter (param: Type)
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "type") == 0) {
                return ExtractTypeText(child, content);
            }
        }
        return "parameter";
    }
    
    static vector<string> ExtractTypedParameterModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        modifiers.push_back("typed_parameter");
        
        // Check for default values
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "=") == 0) {
                modifiers.push_back("has_default");
                break;
            }
        }
        
        return modifiers;
    }
    
    static string ExtractTypeText(TSNode node, const string& content) {
        uint32_t start = ts_node_start_byte(node);
        uint32_t end = ts_node_end_byte(node);
        
        if (start < content.length() && end <= content.length() && end > start) {
            return content.substr(start, end - start);
        }
        
        return "type";
    }
    
    static vector<string> ExtractTypeModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        modifiers.push_back("type_annotation");
        
        // Check parent context to understand where this type is used
        TSNode parent = ts_node_parent(node);
        if (!ts_node_is_null(parent)) {
            string parent_type = ts_node_type(parent);
            
            if (parent_type == "typed_parameter") {
                modifiers.push_back("parameter_type");
            } else if (parent_type == "annotated_assignment") {
                modifiers.push_back("variable_type");
            } else if (parent_type == "function_definition") {
                modifiers.push_back("return_type");
            }
        }
        
        // Check for generic types
        string type_text = ExtractTypeText(node, content);
        if (type_text.find('[') != string::npos) {
            modifiers.push_back("generic_type");
        }
        if (type_text.find("Optional") != string::npos) {
            modifiers.push_back("optional_type");
        }
        if (type_text.find("Union") != string::npos) {
            modifiers.push_back("union_type");
        }
        
        return modifiers;
    }
    
    static string ExtractIdentifierType(TSNode node, const string& content) {
        // For identifiers, try to infer type from context
        TSNode parent = ts_node_parent(node);
        if (!ts_node_is_null(parent)) {
            string parent_type = ts_node_type(parent);
            
            if (parent_type == "assignment") {
                return ExtractAssignmentType(parent, content);
            } else if (parent_type == "annotated_assignment") {
                return ExtractAnnotatedAssignmentType(parent, content);
            } else if (parent_type == "attribute") {
                return "attribute_access";
            } else if (parent_type == "call") {
                return "function_call";
            } else if (parent_type == "import_statement" || parent_type == "import_from_statement") {
                return "import";
            } else if (parent_type == "class_definition") {
                return "class_name";
            } else if (parent_type == "function_definition") {
                return "function_name";
            }
        }
        
        return "identifier";
    }
    
    static vector<string> ExtractIdentifierModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        
        TSNode parent = ts_node_parent(node);
        if (!ts_node_is_null(parent)) {
            string parent_type = ts_node_type(parent);
            modifiers.push_back("in_" + parent_type);
            
            // Add specific context modifiers
            if (parent_type == "assignment") {
                modifiers.push_back("assignment_target");
            } else if (parent_type == "call") {
                modifiers.push_back("function_call");
            } else if (parent_type == "attribute") {
                modifiers.push_back("attribute_access");
            } else if (parent_type == "import_statement") {
                modifiers.push_back("import_name");
            } else if (parent_type == "import_from_statement") {
                modifiers.push_back("import_from");
            }
        }
        
        return modifiers;
    }
    
    static string ExtractDottedNameType(TSNode node, const string& content) {
        // Dotted names are typically for imports or module references
        TSNode parent = ts_node_parent(node);
        if (!ts_node_is_null(parent)) {
            string parent_type = ts_node_type(parent);
            
            if (parent_type == "import_statement") {
                return "module_import";
            } else if (parent_type == "import_from_statement") {
                return "module_from_import";
            } else if (parent_type == "attribute") {
                return "qualified_attribute";
            }
        }
        
        return "qualified_name";
    }
    
    static vector<string> ExtractDottedNameModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        modifiers.push_back("dotted_name");
        
        TSNode parent = ts_node_parent(node);
        if (!ts_node_is_null(parent)) {
            string parent_type = ts_node_type(parent);
            modifiers.push_back("in_" + parent_type);
        }
        
        return modifiers;
    }
    
    static string ExtractAssignmentType(TSNode node, const string& content) {
        // For assignments, try to infer type from right-hand side
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "=") == 0 && i + 1 < child_count) {
                // Next child is the value being assigned
                TSNode value = ts_node_child(node, i + 1);
                return InferTypeFromValue(value, content);
            }
        }
        
        return "assignment";
    }
    
    static vector<string> ExtractAssignmentModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        modifiers.push_back("assignment");
        
        // Check if it's a multiple assignment
        uint32_t child_count = ts_node_child_count(node);
        int assignment_count = 0;
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            if (strcmp(ts_node_type(child), "=") == 0) {
                assignment_count++;
            }
        }
        
        if (assignment_count > 1) {
            modifiers.push_back("multiple_assignment");
        }
        
        return modifiers;
    }
    
    static string ExtractAnnotatedAssignmentType(TSNode node, const string& content) {
        // For annotated assignments, extract the type annotation
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "type") == 0) {
                return ExtractTypeText(child, content);
            }
        }
        
        return "annotated_assignment";
    }
    
    static vector<string> ExtractAnnotatedAssignmentModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        modifiers.push_back("annotated_assignment");
        modifiers.push_back("type_annotated");
        
        // Check if it has a value assignment too
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            if (strcmp(ts_node_type(child), "=") == 0) {
                modifiers.push_back("with_value");
                break;
            }
        }
        
        return modifiers;
    }
    
    static string ExtractAttributeType(TSNode node, const string& content) {
        // For attribute access like obj.attr
        return "attribute_access";
    }
    
    static vector<string> ExtractAttributeModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        modifiers.push_back("attribute");
        
        // Check if it's part of a call or assignment
        TSNode parent = ts_node_parent(node);
        if (!ts_node_is_null(parent)) {
            string parent_type = ts_node_type(parent);
            
            if (parent_type == "call") {
                modifiers.push_back("method_call");
            } else if (parent_type == "assignment") {
                modifiers.push_back("attribute_assignment");
            }
        }
        
        return modifiers;
    }
    
    static string ExtractSubscriptType(TSNode node, const string& content) {
        // For subscript access like obj[key]
        return "subscript_access";
    }
    
    static vector<string> ExtractSubscriptModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        modifiers.push_back("subscript");
        
        // Analyze the subscript type
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            string child_type = ts_node_type(child);
            
            if (child_type == "integer") {
                modifiers.push_back("integer_index");
            } else if (child_type == "string") {
                modifiers.push_back("string_index");
            } else if (child_type == "slice") {
                modifiers.push_back("slice_access");
            }
        }
        
        return modifiers;
    }
    
    static string ExtractComprehensionType(TSNode node, const string& content) {
        string node_type = ts_node_type(node);
        
        if (node_type == "list_comprehension") {
            return "list_comprehension";
        } else if (node_type == "set_comprehension") {
            return "set_comprehension";
        } else if (node_type == "dictionary_comprehension") {
            return "dict_comprehension";
        } else if (node_type == "generator_expression") {
            return "generator_expression";
        }
        
        return "comprehension";
    }
    
    static vector<string> ExtractComprehensionModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        modifiers.push_back("comprehension");
        
        string node_type = ts_node_type(node);
        modifiers.push_back(node_type);
        
        // Check for conditions
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            if (strcmp(ts_node_type(child), "if_clause") == 0) {
                modifiers.push_back("conditional");
                break;
            }
        }
        
        return modifiers;
    }
    
    static string InferTypeFromValue(TSNode value_node, const string& content) {
        string value_type = ts_node_type(value_node);
        
        if (value_type == "integer") {
            return "int";
        } else if (value_type == "float") {
            return "float";
        } else if (value_type == "string") {
            return "str";
        } else if (value_type == "true" || value_type == "false") {
            return "bool";
        } else if (value_type == "none") {
            return "None";
        } else if (value_type == "list") {
            return "list";
        } else if (value_type == "dictionary") {
            return "dict";
        } else if (value_type == "set") {
            return "set";
        } else if (value_type == "tuple") {
            return "tuple";
        } else if (value_type == "call") {
            // Try to extract function name for call inference
            uint32_t child_count = ts_node_child_count(value_node);
            if (child_count > 0) {
                TSNode func_node = ts_node_child(value_node, 0);
                uint32_t start = ts_node_start_byte(func_node);
                uint32_t end = ts_node_end_byte(func_node);
                if (start < content.length() && end <= content.length()) {
                    return content.substr(start, end - start);
                }
            }
            return "function_call";
        } else if (value_type == "attribute") {
            // For attribute access like obj.method
            uint32_t start = ts_node_start_byte(value_node);
            uint32_t end = ts_node_end_byte(value_node);
            if (start < content.length() && end <= content.length()) {
                return content.substr(start, end - start);
            }
            return "attribute";
        }
        
        return "inferred";
    }
};

// Specialization for FUNCTION_CALL (Python function calls)
template<>
struct PythonNativeExtractor<NativeExtractionStrategy::FUNCTION_CALL> {
    static NativeContext Extract(TSNode node, const string& content) {
        return UnifiedFunctionCallExtractor<PythonLanguageTag>::Extract(node, content);
    }
};

// Specialization for FUNCTION_WITH_DECORATORS (Python functions with @decorators)
template<>
struct PythonNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_DECORATORS> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;
        
        try {
            string node_type = ts_node_type(node);
            
            if (node_type == "function_definition" || node_type == "async_function_definition") {
                // Start with basic function extraction
                context = PythonNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS>::Extract(node, content);
                
                // Add decorators and advanced modifiers
                vector<string> decorators = ExtractPythonFunctionDecorators(node, content);
                vector<string> advanced_modifiers = ExtractPythonAdvancedModifiers(node, content);
                
                // Combine existing modifiers with decorators and advanced modifiers
                context.modifiers.insert(context.modifiers.end(), decorators.begin(), decorators.end());
                context.modifiers.insert(context.modifiers.end(), advanced_modifiers.begin(), advanced_modifiers.end());
                
                // Enhance signature type with decorator info
                if (!decorators.empty()) {
                    context.signature_type = "decorated_" + context.signature_type;
                }
                
                // Handle async functions
                if (node_type == "async_function_definition") {
                    context.signature_type = "async_" + context.signature_type;
                    context.modifiers.insert(context.modifiers.begin(), "async");
                }
            } else if (node_type == "decorated_definition") {
                context = ExtractDecoratedFunction(node, content);
            } else {
                // Fallback to basic function extraction
                context = PythonNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS>::Extract(node, content);
            }
        } catch (...) {
            context.signature_type = "";
            context.parameters.clear();
            context.modifiers.clear();
        }
        
        return context;
    }
    
private:
    static vector<string> ExtractPythonFunctionDecorators(TSNode node, const string& content) {
        vector<string> decorators;
        
        // Check if this function has decorators (they appear as siblings before the function)
        TSNode parent = ts_node_parent(node);
        if (!ts_node_is_null(parent)) {
            string parent_type = ts_node_type(parent);
            
            if (parent_type == "decorated_definition") {
                // Get decorators from the decorated_definition parent
                uint32_t child_count = ts_node_child_count(parent);
                for (uint32_t i = 0; i < child_count; i++) {
                    TSNode child = ts_node_child(parent, i);
                    const char* child_type = ts_node_type(child);
                    
                    if (strcmp(child_type, "decorator") == 0) {
                        string decorator_text = ExtractNodeText(child, content);
                        if (!decorator_text.empty()) {
                            decorators.push_back(decorator_text);
                        }
                    }
                }
            }
        }
        
        return decorators;
    }
    
    static vector<string> ExtractPythonAdvancedModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        
        // Check for function-specific modifiers
        string node_type = ts_node_type(node);
        if (node_type == "async_function_definition") {
            modifiers.push_back("async");
        }
        
        // Check for special method names
        string function_name = ExtractFunctionName(node, content);
        if (!function_name.empty()) {
            if (function_name.find("__") == 0 && function_name.rfind("__") == function_name.length() - 2) {
                modifiers.push_back("dunder_method");
            } else if (function_name.find("_") == 0) {
                modifiers.push_back("private_method");
            }
        }
        
        // Check for property-like decorators
        TSNode parent = ts_node_parent(node);
        if (!ts_node_is_null(parent)) {
            string parent_type = ts_node_type(parent);
            if (parent_type == "decorated_definition") {
                string parent_text = ExtractNodeText(parent, content);
                
                if (parent_text.find("@property") != string::npos) {
                    modifiers.push_back("property");
                } else if (parent_text.find("@staticmethod") != string::npos) {
                    modifiers.push_back("staticmethod");
                } else if (parent_text.find("@classmethod") != string::npos) {
                    modifiers.push_back("classmethod");
                } else if (parent_text.find("@abstractmethod") != string::npos) {
                    modifiers.push_back("abstractmethod");
                }
            }
        }
        
        // Check for type annotations
        if (HasTypeAnnotations(node, content)) {
            modifiers.push_back("type_annotated");
        }
        
        // Check for yield (generator function)
        if (IsGeneratorFunction(node, content)) {
            modifiers.push_back("generator");
        }
        
        return modifiers;
    }
    
    static NativeContext ExtractDecoratedFunction(TSNode node, const string& content) {
        NativeContext context;
        
        // Find the actual function definition within the decorated_definition
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            string child_type = ts_node_type(child);
            
            if (child_type == "function_definition" || child_type == "async_function_definition") {
                // Extract basic function info
                context = PythonNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS>::Extract(child, content);
                
                // Add decorators
                vector<string> decorators = ExtractDecoratorsFromDecoratedDefinition(node, content);
                context.modifiers.insert(context.modifiers.end(), decorators.begin(), decorators.end());
                
                // Enhance signature type
                if (!decorators.empty()) {
                    context.signature_type = "decorated_" + context.signature_type;
                }
                
                if (child_type == "async_function_definition") {
                    context.signature_type = "async_" + context.signature_type;
                    context.modifiers.insert(context.modifiers.begin(), "async");
                }
                
                break;
            }
        }
        
        return context;
    }
    
    static vector<string> ExtractDecoratorsFromDecoratedDefinition(TSNode node, const string& content) {
        vector<string> decorators;
        
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "decorator") == 0) {
                string decorator_text = ExtractNodeText(child, content);
                if (!decorator_text.empty()) {
                    decorators.push_back(decorator_text);
                }
            }
        }
        
        return decorators;
    }
    
    static string ExtractFunctionName(TSNode node, const string& content) {
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "identifier") == 0) {
                return ExtractNodeText(child, content);
            }
        }
        
        return "";
    }
    
    static bool HasTypeAnnotations(TSNode node, const string& content) {
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "parameters") == 0) {
                // Check if any parameter has type annotations
                uint32_t param_count = ts_node_child_count(child);
                for (uint32_t j = 0; j < param_count; j++) {
                    TSNode param = ts_node_child(child, j);
                    string param_type = ts_node_type(param);
                    
                    if (param_type == "typed_parameter" || param_type == "typed_default_parameter") {
                        return true;
                    }
                }
            } else if (strcmp(child_type, "->") == 0) {
                // Function has return type annotation
                return true;
            }
        }
        
        return false;
    }
    
    static bool IsGeneratorFunction(TSNode node, const string& content) {
        string node_text = ExtractNodeText(node, content);
        return node_text.find("yield") != string::npos;
    }
    
    static string ExtractNodeText(TSNode node, const string& content) {
        if (ts_node_is_null(node)) {
            return "";
        }
        
        uint32_t start = ts_node_start_byte(node);
        uint32_t end = ts_node_end_byte(node);
        
        if (start < content.length() && end <= content.length() && end > start) {
            return content.substr(start, end - start);
        }
        
        return "";
    }
};

// Specialization for IMPORT_STATEMENT (Python import statements)
template<>
struct PythonNativeExtractor<NativeExtractionStrategy::IMPORT_STATEMENT> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;
        
        try {
            string node_type = ts_node_type(node);
            
            if (node_type == "import_statement") {
                context.signature_type = ExtractImportType(node, content);
                context.modifiers = ExtractImportModifiers(node, content);
            } else if (node_type == "import_from_statement") {
                context.signature_type = ExtractImportFromType(node, content);
                context.modifiers = ExtractImportFromModifiers(node, content);
            } else if (node_type == "dotted_name") {
                context.signature_type = ExtractDottedImportType(node, content);
                context.modifiers = ExtractDottedImportModifiers(node, content);
            } else if (node_type == "aliased_import") {
                context.signature_type = ExtractAliasedImportType(node, content);
                context.modifiers = ExtractAliasedImportModifiers(node, content);
            } else if (node_type == "wildcard_import") {
                context.signature_type = "wildcard_import";
                context.modifiers.push_back("wildcard");
            } else {
                context.signature_type = "import";
                context.modifiers.push_back("import_statement");
            }
        } catch (...) {
            context.signature_type = "";
            context.modifiers.clear();
        }
        
        return context;
    }
    
private:
    static string ExtractImportType(TSNode node, const string& content) {
        // For regular imports: import module
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            string child_type = ts_node_type(child);
            
            if (child_type == "dotted_name" || child_type == "identifier") {
                string module_name = ExtractNodeText(child, content);
                if (!module_name.empty()) {
                    return "import_" + module_name;
                }
            } else if (child_type == "aliased_import") {
                return "aliased_import";
            }
        }
        
        return "import";
    }
    
    static vector<string> ExtractImportModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        modifiers.push_back("import_statement");
        
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            string child_type = ts_node_type(child);
            
            if (child_type == "dotted_name") {
                modifiers.push_back("dotted_import");
                string module_name = ExtractNodeText(child, content);
                if (module_name.find(".") != string::npos) {
                    modifiers.push_back("nested_module");
                }
            } else if (child_type == "aliased_import") {
                modifiers.push_back("aliased");
            }
        }
        
        return modifiers;
    }
    
    static string ExtractImportFromType(TSNode node, const string& content) {
        // For from imports: from module import item
        string module_name = "";
        string import_items = "";
        
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            string child_type = ts_node_type(child);
            
            if (child_type == "dotted_name" || child_type == "identifier") {
                if (module_name.empty()) {
                    module_name = ExtractNodeText(child, content);
                }
            } else if (child_type == "import_list") {
                // Extract what's being imported
                uint32_t import_count = ts_node_child_count(child);
                for (uint32_t j = 0; j < import_count; j++) {
                    TSNode import_item = ts_node_child(child, j);
                    string import_type = ts_node_type(import_item);
                    
                    if (import_type == "identifier" || import_type == "aliased_import") {
                        if (!import_items.empty()) {
                            import_items += ", ";
                        }
                        import_items += ExtractNodeText(import_item, content);
                    }
                }
            } else if (child_type == "wildcard_import") {
                import_items = "*";
            }
        }
        
        if (!module_name.empty()) {
            return "from_" + module_name;
        }
        
        return "from_import";
    }
    
    static vector<string> ExtractImportFromModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        modifiers.push_back("from_import");
        
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            string child_type = ts_node_type(child);
            
            if (child_type == "dotted_name") {
                modifiers.push_back("from_dotted_module");
                string module_name = ExtractNodeText(child, content);
                if (module_name.find(".") != string::npos) {
                    modifiers.push_back("nested_module");
                }
            } else if (child_type == "relative_import") {
                modifiers.push_back("relative_import");
            } else if (child_type == "import_list") {
                modifiers.push_back("specific_imports");
                
                // Count imported items
                uint32_t import_count = ts_node_child_count(child);
                int item_count = 0;
                bool has_aliases = false;
                
                for (uint32_t j = 0; j < import_count; j++) {
                    TSNode import_item = ts_node_child(child, j);
                    string import_type = ts_node_type(import_item);
                    
                    if (import_type == "identifier") {
                        item_count++;
                    } else if (import_type == "aliased_import") {
                        item_count++;
                        has_aliases = true;
                    }
                }
                
                if (item_count > 1) {
                    modifiers.push_back("multiple_imports");
                }
                if (has_aliases) {
                    modifiers.push_back("with_aliases");
                }
            } else if (child_type == "wildcard_import") {
                modifiers.push_back("wildcard_import");
            }
        }
        
        return modifiers;
    }
    
    static string ExtractDottedImportType(TSNode node, const string& content) {
        string dotted_name = ExtractNodeText(node, content);
        if (!dotted_name.empty()) {
            return "dotted_" + dotted_name;
        }
        return "dotted_import";
    }
    
    static vector<string> ExtractDottedImportModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        modifiers.push_back("dotted_name");
        
        string dotted_name = ExtractNodeText(node, content);
        if (!dotted_name.empty()) {
            int dot_count = 0;
            for (char c : dotted_name) {
                if (c == '.') dot_count++;
            }
            
            if (dot_count == 1) {
                modifiers.push_back("single_level");
            } else if (dot_count > 1) {
                modifiers.push_back("deep_nested");
            }
        }
        
        return modifiers;
    }
    
    static string ExtractAliasedImportType(TSNode node, const string& content) {
        // For aliased imports: import module as alias
        string original_name = "";
        string alias_name = "";
        
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            string child_type = ts_node_type(child);
            
            if (child_type == "identifier" || child_type == "dotted_name") {
                if (original_name.empty()) {
                    original_name = ExtractNodeText(child, content);
                } else {
                    alias_name = ExtractNodeText(child, content);
                }
            }
        }
        
        if (!original_name.empty()) {
            return "aliased_" + original_name;
        }
        
        return "aliased_import";
    }
    
    static vector<string> ExtractAliasedImportModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        modifiers.push_back("aliased_import");
        
        // Extract original and alias names
        string original_name = "";
        string alias_name = "";
        
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            string child_type = ts_node_type(child);
            
            if (child_type == "identifier" || child_type == "dotted_name") {
                if (original_name.empty()) {
                    original_name = ExtractNodeText(child, content);
                } else {
                    alias_name = ExtractNodeText(child, content);
                }
            }
        }
        
        if (!original_name.empty()) {
            modifiers.push_back("original_" + original_name);
        }
        if (!alias_name.empty()) {
            modifiers.push_back("alias_" + alias_name);
        }
        
        return modifiers;
    }
    
    static string ExtractNodeText(TSNode node, const string& content) {
        if (ts_node_is_null(node)) {
            return "";
        }
        
        uint32_t start = ts_node_start_byte(node);
        uint32_t end = ts_node_end_byte(node);
        
        if (start < content.length() && end <= content.length() && end > start) {
            return content.substr(start, end - start);
        }
        
        return "";
    }
};

} // namespace duckdb