#pragma once

#include "native_context_extraction.hpp"
#include "function_call_extractor.hpp"
#include <tree_sitter/api.h>

namespace duckdb {

//==============================================================================
// Dart-Specific Native Context Extractors
// Dart: Client-optimized language with sound null safety and async support
//==============================================================================

// Forward declaration for DartAdapter
class DartAdapter;

// Base template for Dart extractors - default returns empty context
template<NativeExtractionStrategy Strategy>
struct DartNativeExtractor {
    static NativeContext Extract(TSNode node, const string& content) {
        return NativeContext(); // Default: no extraction
    }
};

//==============================================================================
// Helper functions for Dart-specific extraction
//==============================================================================

namespace dart_helpers {

inline string ExtractNodeText(TSNode node, const string& content) {
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

// Extract return type from Dart function signature
// Dart has return type before function name: ReturnType functionName(params)
inline string ExtractDartReturnType(TSNode node, const string& content) {
    uint32_t child_count = ts_node_child_count(node);

    // Look for return type node (type annotations in Dart)
    for (uint32_t i = 0; i < child_count; i++) {
        TSNode child = ts_node_child(node, i);
        const char* child_type = ts_node_type(child);

        // Check for various type patterns in Dart
        if (strcmp(child_type, "type_identifier") == 0 ||
            strcmp(child_type, "void_type") == 0 ||
            strcmp(child_type, "function_type") == 0 ||
            strcmp(child_type, "nullable_type") == 0 ||
            strcmp(child_type, "type_arguments") == 0) {
            return ExtractNodeText(child, content);
        }
    }

    // Check parent for return type context
    TSNode parent = ts_node_parent(node);
    if (!ts_node_is_null(parent)) {
        uint32_t parent_count = ts_node_child_count(parent);
        for (uint32_t i = 0; i < parent_count && i < 10; i++) {
            TSNode sibling = ts_node_child(parent, i);
            const char* sibling_type = ts_node_type(sibling);

            if (strcmp(sibling_type, "type_identifier") == 0 ||
                strcmp(sibling_type, "void_type") == 0 ||
                strcmp(sibling_type, "function_type") == 0 ||
                strcmp(sibling_type, "nullable_type") == 0) {
                return ExtractNodeText(sibling, content);
            }
        }
    }

    return ""; // Type inferred or void
}

// Extract parameters from Dart formal_parameter_list
inline vector<ParameterInfo> ExtractDartParameters(TSNode node, const string& content) {
    vector<ParameterInfo> params;

    // Find formal_parameter_list node
    uint32_t child_count = ts_node_child_count(node);
    for (uint32_t i = 0; i < child_count; i++) {
        TSNode child = ts_node_child(node, i);
        const char* child_type = ts_node_type(child);

        if (strcmp(child_type, "formal_parameter_list") == 0) {
            uint32_t param_count = ts_node_child_count(child);

            for (uint32_t j = 0; j < param_count; j++) {
                TSNode param_node = ts_node_child(child, j);
                const char* param_type = ts_node_type(param_node);

                ParameterInfo param;
                bool is_valid = false;

                if (strcmp(param_type, "formal_parameter") == 0 ||
                    strcmp(param_type, "constructor_param") == 0 ||
                    strcmp(param_type, "super_formal_parameter") == 0) {
                    // Extract parameter details
                    uint32_t inner_count = ts_node_child_count(param_node);

                    for (uint32_t k = 0; k < inner_count; k++) {
                        TSNode inner = ts_node_child(param_node, k);
                        const char* inner_type = ts_node_type(inner);

                        if (strcmp(inner_type, "identifier") == 0) {
                            param.name = ExtractNodeText(inner, content);
                            is_valid = true;
                        } else if (strcmp(inner_type, "type_identifier") == 0 ||
                                   strcmp(inner_type, "nullable_type") == 0 ||
                                   strcmp(inner_type, "function_type") == 0) {
                            param.type = ExtractNodeText(inner, content);
                        } else if (strcmp(inner_type, "final_builtin") == 0 ||
                                   strcmp(inner_type, "const_builtin") == 0) {
                            param.annotations = ExtractNodeText(inner, content);
                        } else if (strcmp(inner_type, "this") == 0) {
                            // Constructor parameter: this.x
                            param.annotations = "this";
                        } else if (strcmp(inner_type, "super") == 0) {
                            // Super formal parameter: super.x
                            param.annotations = "super";
                        }
                    }
                } else if (strcmp(param_type, "optional_formal_parameters") == 0) {
                    // Handle optional parameters [a, b] or {a, b}
                    uint32_t opt_count = ts_node_child_count(param_node);

                    for (uint32_t k = 0; k < opt_count; k++) {
                        TSNode opt_param = ts_node_child(param_node, k);
                        const char* opt_type = ts_node_type(opt_param);

                        if (strcmp(opt_type, "formal_parameter") == 0) {
                            ParameterInfo opt_info;
                            opt_info.is_optional = true;

                            uint32_t opt_inner_count = ts_node_child_count(opt_param);
                            for (uint32_t m = 0; m < opt_inner_count; m++) {
                                TSNode opt_inner = ts_node_child(opt_param, m);
                                const char* opt_inner_type = ts_node_type(opt_inner);

                                if (strcmp(opt_inner_type, "identifier") == 0) {
                                    opt_info.name = ExtractNodeText(opt_inner, content);
                                } else if (strcmp(opt_inner_type, "type_identifier") == 0 ||
                                           strcmp(opt_inner_type, "nullable_type") == 0) {
                                    opt_info.type = ExtractNodeText(opt_inner, content);
                                }
                            }

                            if (!opt_info.name.empty()) {
                                params.push_back(opt_info);
                            }
                        }
                    }
                    continue;
                }

                if (is_valid) {
                    params.push_back(param);
                }
            }
            break;
        }
    }

    return params;
}

// Extract Dart modifiers (async, sync*, async*, static, abstract, etc.)
inline vector<string> ExtractDartModifiers(TSNode node, const string& content) {
    vector<string> modifiers;

    uint32_t child_count = ts_node_child_count(node);
    for (uint32_t i = 0; i < child_count; i++) {
        TSNode child = ts_node_child(node, i);
        const char* child_type = ts_node_type(child);

        // Check for various modifier keywords
        if (strcmp(child_type, "abstract") == 0 ||
            strcmp(child_type, "static") == 0 ||
            strcmp(child_type, "external") == 0 ||
            strcmp(child_type, "covariant") == 0 ||
            strcmp(child_type, "late") == 0 ||
            strcmp(child_type, "final_builtin") == 0 ||
            strcmp(child_type, "const_builtin") == 0) {
            modifiers.push_back(ExtractNodeText(child, content));
        }
    }

    // Check parent for additional modifiers
    TSNode parent = ts_node_parent(node);
    if (!ts_node_is_null(parent)) {
        uint32_t parent_count = ts_node_child_count(parent);
        for (uint32_t i = 0; i < parent_count && i < 15; i++) {
            TSNode sibling = ts_node_child(parent, i);
            const char* sibling_type = ts_node_type(sibling);

            if (strcmp(sibling_type, "abstract") == 0 ||
                strcmp(sibling_type, "static") == 0 ||
                strcmp(sibling_type, "external") == 0 ||
                strcmp(sibling_type, "base") == 0 ||
                strcmp(sibling_type, "sealed") == 0 ||
                strcmp(sibling_type, "interface") == 0 ||
                strcmp(sibling_type, "mixin") == 0 ||
                strcmp(sibling_type, "annotation") == 0) {
                string mod_text = ExtractNodeText(sibling, content);
                if (!mod_text.empty()) {
                    modifiers.push_back(mod_text);
                }
            }
        }
    }

    return modifiers;
}

// Extract parent types from Dart class inheritance clauses
inline vector<ParameterInfo> ExtractDartParentTypes(TSNode node, const string& content,
                                                     bool& has_extends, bool& has_implements, bool& has_with) {
    vector<ParameterInfo> parents;
    has_extends = false;
    has_implements = false;
    has_with = false;

    uint32_t child_count = ts_node_child_count(node);
    for (uint32_t i = 0; i < child_count; i++) {
        TSNode child = ts_node_child(node, i);
        const char* child_type = ts_node_type(child);

        if (strcmp(child_type, "superclass") == 0) {
            // extends clause - extract parent class
            // Note: "mixins" (with clause) is INSIDE superclass in tree-sitter Dart grammar
            has_extends = true;
            uint32_t super_count = ts_node_child_count(child);
            for (uint32_t j = 0; j < super_count; j++) {
                TSNode super_child = ts_node_child(child, j);
                const char* super_type = ts_node_type(super_child);

                // Skip "extends" keyword
                if (strcmp(super_type, "extends") == 0) continue;

                if (strcmp(super_type, "type_identifier") == 0 ||
                    strcmp(super_type, "identifier") == 0) {
                    string type_name = ExtractNodeText(super_child, content);
                    if (!type_name.empty()) {
                        parents.push_back({type_name, ""});
                    }
                } else if (strcmp(super_type, "mixins") == 0) {
                    // with clause - extract mixins (nested inside superclass)
                    has_with = true;
                    uint32_t mixin_count = ts_node_child_count(super_child);
                    for (uint32_t k = 0; k < mixin_count; k++) {
                        TSNode mixin_child = ts_node_child(super_child, k);
                        const char* mixin_type = ts_node_type(mixin_child);

                        // Skip "with" keyword and punctuation
                        if (strcmp(mixin_type, "with") == 0 ||
                            strcmp(mixin_type, ",") == 0) continue;

                        if (strcmp(mixin_type, "type_identifier") == 0 ||
                            strcmp(mixin_type, "identifier") == 0) {
                            string type_name = ExtractNodeText(mixin_child, content);
                            if (!type_name.empty()) {
                                parents.push_back({type_name, ""});
                            }
                        }
                    }
                }
            }
        } else if (strcmp(child_type, "interfaces") == 0) {
            // implements clause - extract interfaces
            has_implements = true;
            uint32_t impl_count = ts_node_child_count(child);
            for (uint32_t j = 0; j < impl_count; j++) {
                TSNode impl_child = ts_node_child(child, j);
                const char* impl_type = ts_node_type(impl_child);

                // Skip "implements" keyword and punctuation
                if (strcmp(impl_type, "implements") == 0 ||
                    strcmp(impl_type, ",") == 0) continue;

                if (strcmp(impl_type, "type_identifier") == 0 ||
                    strcmp(impl_type, "identifier") == 0) {
                    string type_name = ExtractNodeText(impl_child, content);
                    if (!type_name.empty()) {
                        parents.push_back({type_name, ""});
                    }
                }
            }
        } else if (strcmp(child_type, "mixins") == 0) {
            // with clause - extract mixins
            has_with = true;
            uint32_t mixin_count = ts_node_child_count(child);
            for (uint32_t j = 0; j < mixin_count; j++) {
                TSNode mixin_child = ts_node_child(child, j);
                const char* mixin_type = ts_node_type(mixin_child);

                // Skip "with" keyword and punctuation
                if (strcmp(mixin_type, "with") == 0 ||
                    strcmp(mixin_type, ",") == 0) continue;

                if (strcmp(mixin_type, "type_identifier") == 0 ||
                    strcmp(mixin_type, "identifier") == 0) {
                    string type_name = ExtractNodeText(mixin_child, content);
                    if (!type_name.empty()) {
                        parents.push_back({type_name, ""});
                    }
                }
            }
        }
    }

    return parents;
}

// Extract class modifiers (excluding inheritance which goes in parameters now)
// is_mixin_declaration: true if this is a mixin_declaration node (to avoid adding "mixin" as a modifier)
inline vector<string> ExtractDartClassModifiers(TSNode node, const string& content,
                                                 bool has_extends, bool has_implements, bool has_with,
                                                 bool is_mixin_declaration = false) {
    vector<string> modifiers;

    // Add inheritance keywords
    if (has_extends) {
        modifiers.push_back("extends");
    }
    if (has_implements) {
        modifiers.push_back("implements");
    }
    if (has_with) {
        modifiers.push_back("with");
    }

    uint32_t child_count = ts_node_child_count(node);
    for (uint32_t i = 0; i < child_count; i++) {
        TSNode child = ts_node_child(node, i);
        const char* child_type = ts_node_type(child);

        if (strcmp(child_type, "abstract") == 0 ||
            strcmp(child_type, "base") == 0 ||
            strcmp(child_type, "sealed") == 0 ||
            strcmp(child_type, "interface") == 0 ||
            strcmp(child_type, "final") == 0) {
            modifiers.push_back(child_type);
        } else if (strcmp(child_type, "mixin") == 0 && !is_mixin_declaration) {
            // Only add "mixin" as modifier for "mixin class" declarations, not for mixin_declaration
            modifiers.push_back(child_type);
        } else if (strcmp(child_type, "annotation") == 0) {
            string ann_text = ExtractNodeText(child, content);
            if (!ann_text.empty()) {
                modifiers.push_back(ann_text);
            }
        } else if (strcmp(child_type, "type_parameters") == 0) {
            // Generic type parameters
            string generics = ExtractNodeText(child, content);
            if (!generics.empty()) {
                modifiers.push_back("generics " + generics);
            }
        }
    }

    return modifiers;
}

} // namespace dart_helpers

//==============================================================================
// Specialization for FUNCTION_WITH_PARAMS (Dart functions and methods)
//==============================================================================

template<>
struct DartNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;

        // Extract return type
        context.signature_type = dart_helpers::ExtractDartReturnType(node, content);

        // Extract parameters
        context.parameters = dart_helpers::ExtractDartParameters(node, content);

        // Extract modifiers
        context.modifiers = dart_helpers::ExtractDartModifiers(node, content);

        // Check for async/sync modifiers in function body
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);

            if (strcmp(child_type, "function_body") == 0) {
                // Check for async/sync* markers
                uint32_t body_count = ts_node_child_count(child);
                for (uint32_t j = 0; j < body_count && j < 5; j++) {
                    TSNode body_child = ts_node_child(child, j);
                    string text = dart_helpers::ExtractNodeText(body_child, content);
                    if (text == "async" || text == "async*" || text == "sync*") {
                        context.modifiers.push_back(text);
                    }
                }
            }
        }

        return context;
    }
};

//==============================================================================
// Specialization for ASYNC_FUNCTION (Dart async functions)
//==============================================================================

template<>
struct DartNativeExtractor<NativeExtractionStrategy::ASYNC_FUNCTION> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context = DartNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS>::Extract(node, content);

        // Ensure async is in modifiers
        bool has_async = false;
        for (const auto& mod : context.modifiers) {
            if (mod == "async" || mod == "async*" || mod == "sync*") {
                has_async = true;
                break;
            }
        }

        if (!has_async) {
            context.modifiers.push_back("async");
        }

        return context;
    }
};

//==============================================================================
// Specialization for ARROW_FUNCTION (Dart lambda expressions)
//==============================================================================

template<>
struct DartNativeExtractor<NativeExtractionStrategy::ARROW_FUNCTION> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;
        context.signature_type = "lambda";

        // Lambda expressions in Dart can have parameters
        context.parameters = dart_helpers::ExtractDartParameters(node, content);

        return context;
    }
};

//==============================================================================
// Specialization for CLASS_WITH_METHODS (Dart classes, enums, mixins, extensions)
//==============================================================================

template<>
struct DartNativeExtractor<NativeExtractionStrategy::CLASS_WITH_METHODS> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;

        const char* node_type = ts_node_type(node);

        // Determine class type
        if (strcmp(node_type, "class_definition") == 0) {
            // Check for sealed, abstract, etc. - modifiers are direct children
            context.signature_type = "class";

            uint32_t child_count = ts_node_child_count(node);
            for (uint32_t i = 0; i < child_count; i++) {
                TSNode child = ts_node_child(node, i);
                const char* child_type = ts_node_type(child);

                if (strcmp(child_type, "abstract") == 0) {
                    context.signature_type = "abstract_class";
                } else if (strcmp(child_type, "sealed") == 0) {
                    context.signature_type = "sealed_class";
                } else if (strcmp(child_type, "base") == 0) {
                    context.signature_type = "base_class";
                } else if (strcmp(child_type, "interface") == 0) {
                    context.signature_type = "interface_class";
                } else if (strcmp(child_type, "final") == 0) {
                    context.signature_type = "final_class";
                }
            }
        } else if (strcmp(node_type, "enum_declaration") == 0) {
            context.signature_type = "enum";
        } else if (strcmp(node_type, "mixin_declaration") == 0) {
            context.signature_type = "mixin";
        } else if (strcmp(node_type, "extension_declaration") == 0) {
            context.signature_type = "extension";
        } else if (strcmp(node_type, "extension_type_declaration") == 0) {
            context.signature_type = "extension_type";
        } else if (strcmp(node_type, "mixin_application_class") == 0) {
            context.signature_type = "mixin_application";
        } else if (strcmp(node_type, "type_alias") == 0) {
            context.signature_type = "typedef";
        } else {
            context.signature_type = "type";
        }

        // Extract parent types into parameters
        bool has_extends = false;
        bool has_implements = false;
        bool has_with = false;
        bool is_mixin_declaration = (strcmp(node_type, "mixin_declaration") == 0);
        context.parameters = dart_helpers::ExtractDartParentTypes(node, content, has_extends, has_implements, has_with);
        context.modifiers = dart_helpers::ExtractDartClassModifiers(node, content, has_extends, has_implements, has_with, is_mixin_declaration);

        return context;
    }
};

//==============================================================================
// Specialization for CLASS_WITH_INHERITANCE (alias for CLASS_WITH_METHODS)
//==============================================================================

template<>
struct DartNativeExtractor<NativeExtractionStrategy::CLASS_WITH_INHERITANCE> {
    static NativeContext Extract(TSNode node, const string& content) {
        return DartNativeExtractor<NativeExtractionStrategy::CLASS_WITH_METHODS>::Extract(node, content);
    }
};

//==============================================================================
// Specialization for VARIABLE_WITH_TYPE (Dart variable declarations)
//==============================================================================

template<>
struct DartNativeExtractor<NativeExtractionStrategy::VARIABLE_WITH_TYPE> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;

        uint32_t child_count = ts_node_child_count(node);

        // Extract type from variable declaration
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);

            if (strcmp(child_type, "type_identifier") == 0 ||
                strcmp(child_type, "nullable_type") == 0 ||
                strcmp(child_type, "function_type") == 0 ||
                strcmp(child_type, "inferred_type") == 0) {
                context.signature_type = dart_helpers::ExtractNodeText(child, content);
            } else if (strcmp(child_type, "final_builtin") == 0) {
                context.modifiers.push_back("final");
            } else if (strcmp(child_type, "const_builtin") == 0) {
                context.modifiers.push_back("const");
            } else if (strcmp(child_type, "late") == 0) {
                context.modifiers.push_back("late");
            } else if (strcmp(child_type, "static") == 0) {
                context.modifiers.push_back("static");
            }
        }

        // Check for var keyword (inferred type)
        if (context.signature_type.empty()) {
            for (uint32_t i = 0; i < child_count; i++) {
                TSNode child = ts_node_child(node, i);
                const char* child_type = ts_node_type(child);

                if (strcmp(child_type, "inferred_type") == 0) {
                    context.signature_type = "var";
                    break;
                }
            }
        }

        return context;
    }
};

//==============================================================================
// Specialization for FUNCTION_CALL (Dart function/method calls)
//==============================================================================

template<>
struct DartNativeExtractor<NativeExtractionStrategy::FUNCTION_CALL> {
    static NativeContext Extract(TSNode node, const string& content) {
        return UnifiedFunctionCallExtractor<DartLanguageTag>::Extract(node, content);
    }
};

//==============================================================================
// Specialization for FUNCTION_WITH_DECORATORS (Dart with annotations)
//==============================================================================

template<>
struct DartNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_DECORATORS> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context = DartNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS>::Extract(node, content);

        // Look for annotations (@override, @deprecated, etc.)
        TSNode parent = ts_node_parent(node);
        if (!ts_node_is_null(parent)) {
            uint32_t parent_count = ts_node_child_count(parent);
            for (uint32_t i = 0; i < parent_count; i++) {
                TSNode sibling = ts_node_child(parent, i);
                const char* sibling_type = ts_node_type(sibling);

                if (strcmp(sibling_type, "annotation") == 0) {
                    string ann_text = dart_helpers::ExtractNodeText(sibling, content);
                    if (!ann_text.empty()) {
                        context.modifiers.push_back(ann_text);
                    }
                }
            }
        }

        // Check within the node itself for annotations
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);

            if (strcmp(child_type, "annotation") == 0) {
                string ann_text = dart_helpers::ExtractNodeText(child, content);
                if (!ann_text.empty()) {
                    context.modifiers.push_back(ann_text);
                }
            }
        }

        return context;
    }
};

} // namespace duckdb
