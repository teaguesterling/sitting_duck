#pragma once

#include "native_context_extraction.hpp"
#include <tree_sitter/api.h>
#include <string>
#include <vector>
#include <set>
#include <map>

using namespace std;

namespace duckdb {

// Forward declaration
class CSSAdapter;

//==============================================================================
// CSS Native Context Extraction
//==============================================================================

template<NativeExtractionStrategy Strategy>
struct CSSNativeExtractor {
    static NativeContext Extract(TSNode node, const string& content);
};

//==============================================================================
// CSS Rule/Selector Extraction (for CSS rules, at-rules, selectors)
//==============================================================================

template<>
struct CSSNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;
        try {
            string node_type = ts_node_type(node);
            
            if (node_type == "rule_set") {
                context.signature_type = "RULE_SET";
                context.parameters = ExtractRuleProperties(node, content);
                context.modifiers = ExtractRuleModifiers(node, content);
            } else if (node_type == "at_rule") {
                context.signature_type = "AT_RULE";
                context.parameters = ExtractAtRuleParameters(node, content);
                context.modifiers = ExtractAtRuleModifiers(node, content);
            } else if (node_type == "media_statement") {
                context.signature_type = "MEDIA_QUERY";
                context.parameters = ExtractMediaFeatures(node, content);
                context.modifiers.push_back("RESPONSIVE");
            } else if (node_type == "keyframes_statement") {
                context.signature_type = "ANIMATION";
                context.parameters = ExtractKeyframeParameters(node, content);
                context.modifiers.push_back("ANIMATED");
            } else if (node_type == "supports_statement") {
                context.signature_type = "FEATURE_QUERY";
                context.parameters = ExtractSupportsParameters(node, content);
                context.modifiers.push_back("PROGRESSIVE");
            } else if (node_type == "call_expression") {
                context.signature_type = "CSS_FUNCTION";
                context.parameters = ExtractFunctionArguments(node, content);
                context.modifiers = ExtractFunctionModifiers(node, content);
            } else if (node_type == "import_statement") {
                context.signature_type = "IMPORT";
                context.parameters = ExtractImportParameters(node, content);
                context.modifiers.push_back("EXTERNAL");
            } else {
                context.signature_type = "CSS";
                context.parameters.clear();
                context.modifiers.clear();
            }
        } catch (...) {
            context.signature_type = "";
            context.parameters.clear();
            context.modifiers.clear();
        }
        return context;
    }

    // Public static methods for CSS extraction
    static vector<ParameterInfo> ExtractRuleProperties(TSNode node, const string& content) {
        vector<ParameterInfo> properties;
        
        // Find the block containing declarations
        TSNode block = FindChildByType(node, "block");
        if (!ts_node_is_null(block)) {
            uint32_t child_count = ts_node_child_count(block);
            for (uint32_t i = 0; i < child_count; i++) {
                TSNode child = ts_node_child(block, i);
                if (string(ts_node_type(child)) == "declaration") {
                    ParameterInfo prop = ExtractDeclarationInfo(child, content);
                    if (!prop.name.empty()) {
                        properties.push_back(prop);
                    }
                }
            }
        }
        
        return properties;
    }
    
    static ParameterInfo ExtractDeclarationInfo(TSNode decl, const string& content) {
        ParameterInfo info;
        info.is_optional = false;
        info.is_variadic = false;
        
        // Extract property name
        TSNode prop_name = FindChildByType(decl, "property_name");
        if (!ts_node_is_null(prop_name)) {
            info.name = ExtractNodeText(prop_name, content);
        }
        
        // Extract property value type
        vector<string> value_types;
        uint32_t child_count = ts_node_child_count(decl);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(decl, i);
            string child_type = ts_node_type(child);
            
            if (child_type == "integer_value" || child_type == "float_value") {
                value_types.push_back("NUMBER");
            } else if (child_type == "string_value") {
                value_types.push_back("STRING");
            } else if (child_type == "color_value") {
                value_types.push_back("COLOR");
            } else if (child_type == "call_expression") {
                value_types.push_back("FUNCTION");
            } else if (child_type == "plain_value") {
                value_types.push_back("KEYWORD");
            }
        }
        
        if (!value_types.empty()) {
            info.type = value_types[0]; // Use first type found
        }
        
        return info;
    }
    
    static vector<ParameterInfo> ExtractAtRuleParameters(TSNode node, const string& content) {
        vector<ParameterInfo> params;
        
        // Extract parameters based on at-rule type
        TSNode at_keyword = FindChildByType(node, "at_keyword");
        if (!ts_node_is_null(at_keyword)) {
            string keyword = ExtractNodeText(at_keyword, content);
            
            if (keyword == "@media") {
                params = ExtractMediaFeatures(node, content);
            } else if (keyword == "@import") {
                params = ExtractImportParameters(node, content);
            } else if (keyword == "@keyframes") {
                params = ExtractKeyframeParameters(node, content);
            }
        }
        
        return params;
    }
    
    static vector<ParameterInfo> ExtractMediaFeatures(TSNode node, const string& content) {
        vector<ParameterInfo> features;
        
        // Find media features in the query
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            string child_type = ts_node_type(child);
            
            if (child_type == "feature_name" || child_type == "media_feature_name") {
                ParameterInfo feature;
                feature.name = ExtractNodeText(child, content);
                feature.type = "MEDIA_FEATURE";
                feature.is_optional = false;
                feature.is_variadic = false;
                features.push_back(feature);
            }
        }
        
        return features;
    }
    
    static vector<ParameterInfo> ExtractKeyframeParameters(TSNode node, const string& content) {
        vector<ParameterInfo> params;
        
        // Extract animation name
        TSNode name = FindChildByType(node, "keyframes_name");
        if (!ts_node_is_null(name)) {
            ParameterInfo param;
            param.name = ExtractNodeText(name, content);
            param.type = "ANIMATION_NAME";
            param.is_optional = false;
            param.is_variadic = false;
            params.push_back(param);
        }
        
        return params;
    }
    
    static vector<ParameterInfo> ExtractSupportsParameters(TSNode node, const string& content) {
        vector<ParameterInfo> params;
        
        // Extract feature queries
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            string child_type = ts_node_type(child);
            
            if (child_type == "feature_query") {
                ParameterInfo param;
                param.name = ExtractNodeText(child, content);
                param.type = "FEATURE_QUERY";
                param.is_optional = false;
                param.is_variadic = false;
                params.push_back(param);
            }
        }
        
        return params;
    }
    
    static vector<ParameterInfo> ExtractFunctionArguments(TSNode node, const string& content) {
        vector<ParameterInfo> args;
        
        // Find function arguments
        TSNode arguments = FindChildByType(node, "arguments");
        if (!ts_node_is_null(arguments)) {
            uint32_t child_count = ts_node_child_count(arguments);
            for (uint32_t i = 0; i < child_count; i++) {
                TSNode child = ts_node_child(arguments, i);
                string child_type = ts_node_type(child);
                
                if (child_type != "," && child_type != "(" && child_type != ")") {
                    ParameterInfo arg;
                    arg.name = ExtractNodeText(child, content);
                    arg.type = child_type;
                    arg.is_optional = false;
                    arg.is_variadic = false;
                    args.push_back(arg);
                }
            }
        }
        
        return args;
    }
    
    static vector<ParameterInfo> ExtractImportParameters(TSNode node, const string& content) {
        vector<ParameterInfo> params;
        
        // Extract import URL/path
        TSNode string_val = FindChildByType(node, "string_value");
        if (!ts_node_is_null(string_val)) {
            ParameterInfo param;
            param.name = ExtractNodeText(string_val, content);
            param.type = "IMPORT_PATH";
            param.is_optional = false;
            param.is_variadic = false;
            params.push_back(param);
        }
        
        return params;
    }
    
    static vector<string> ExtractRuleModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        
        // Check for selectors to determine rule type
        TSNode selectors = FindChildByType(node, "selectors");
        if (!ts_node_is_null(selectors)) {
            string selector_text = ExtractNodeText(selectors, content);
            
            if (selector_text.find('.') != string::npos) {
                modifiers.push_back("CLASS_BASED");
            }
            if (selector_text.find('#') != string::npos) {
                modifiers.push_back("ID_BASED");
            }
            if (selector_text.find(':') != string::npos) {
                modifiers.push_back("PSEUDO");
            }
            if (selector_text.find('@') != string::npos) {
                modifiers.push_back("AT_RULE");
            }
        }
        
        return modifiers;
    }
    
    static vector<string> ExtractAtRuleModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        
        TSNode at_keyword = FindChildByType(node, "at_keyword");
        if (!ts_node_is_null(at_keyword)) {
            string keyword = ExtractNodeText(at_keyword, content);
            
            if (keyword == "@media") {
                modifiers.push_back("RESPONSIVE");
            } else if (keyword == "@keyframes") {
                modifiers.push_back("ANIMATED");
            } else if (keyword == "@supports") {
                modifiers.push_back("PROGRESSIVE");
            } else if (keyword == "@import") {
                modifiers.push_back("EXTERNAL");
            }
        }
        
        return modifiers;
    }
    
    static vector<string> ExtractFunctionModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        
        TSNode func_name = FindChildByType(node, "function_name");
        if (!ts_node_is_null(func_name)) {
            string name = ExtractNodeText(func_name, content);
            
            if (name == "calc" || name == "clamp" || name == "min" || name == "max") {
                modifiers.push_back("MATHEMATICAL");
            } else if (name == "var") {
                modifiers.push_back("VARIABLE");
            } else if (name == "url") {
                modifiers.push_back("RESOURCE");
            } else if (name == "rgb" || name == "rgba" || name == "hsl" || name == "hsla") {
                modifiers.push_back("COLOR");
            } else if (name == "linear-gradient" || name == "radial-gradient") {
                modifiers.push_back("GRADIENT");
            }
        }
        
        return modifiers;
    }
    
    static TSNode FindChildByType(TSNode parent, const string& type) {
        uint32_t child_count = ts_node_child_count(parent);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(parent, i);
            if (string(ts_node_type(child)) == type) {
                return child;
            }
        }
        return {0};
    }
    
    static string ExtractNodeText(TSNode node, const string& content) {
        if (ts_node_is_null(node)) {
            return "";
        }
        
        uint32_t start = ts_node_start_byte(node);
        uint32_t end = ts_node_end_byte(node);
        
        if (start >= content.length() || end > content.length() || start >= end) {
            return "";
        }
        
        return content.substr(start, end - start);
    }
};

//==============================================================================
// CSS Variable/Selector Extraction
//==============================================================================

template<>
struct CSSNativeExtractor<NativeExtractionStrategy::VARIABLE_WITH_TYPE> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;
        try {
            string node_type = ts_node_type(node);
            
            if (node_type == "class_selector" || node_type == "class_name") {
                context.signature_type = "CLASS";
                context.parameters.clear();
                context.modifiers.push_back("SELECTOR");
            } else if (node_type == "id_selector" || node_type == "id_name") {
                context.signature_type = "ID";
                context.parameters.clear();
                context.modifiers.push_back("UNIQUE");
            } else if (node_type == "property_name") {
                context.signature_type = "PROPERTY";
                context.parameters.clear();
                context.modifiers = ExtractPropertyModifiers(node, content);
            } else if (node_type == "variable_name") {
                context.signature_type = "CUSTOM_PROPERTY";
                context.parameters.clear();
                context.modifiers.push_back("CSS_VARIABLE");
            } else if (node_type == "identifier") {
                context.signature_type = "IDENTIFIER";
                context.parameters.clear();
                context.modifiers.clear();
            } else if (node_type == "tag_name") {
                context.signature_type = "ELEMENT";
                context.parameters.clear();
                context.modifiers.push_back("HTML_TAG");
            } else if (node_type == "pseudo_class_selector") {
                context.signature_type = "PSEUDO_CLASS";
                context.parameters.clear();
                context.modifiers.push_back("STATE");
            } else if (node_type == "pseudo_element_selector") {
                context.signature_type = "PSEUDO_ELEMENT";
                context.parameters.clear();
                context.modifiers.push_back("VIRTUAL");
            } else if (node_type == "integer_value" || node_type == "float_value") {
                context.signature_type = "NUMBER";
                context.parameters.clear();
                context.modifiers = ExtractNumberModifiers(node, content);
            } else if (node_type == "string_value") {
                context.signature_type = "STRING";
                context.parameters.clear();
                context.modifiers.push_back("LITERAL");
            } else if (node_type == "color_value") {
                context.signature_type = "COLOR";
                context.parameters.clear();
                context.modifiers.push_back("VISUAL");
            } else {
                context.signature_type = "";
                context.parameters.clear();
                context.modifiers.clear();
            }
        } catch (...) {
            context.signature_type = "";
            context.parameters.clear();
            context.modifiers.clear();
        }
        return context;
    }

    // Public static methods for CSS variable extraction
    static vector<string> ExtractPropertyModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        
        string prop_name = CSSNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS>::ExtractNodeText(node, content);
        
        // Categorize CSS properties
        if (prop_name.find("color") != string::npos || prop_name.find("background") != string::npos) {
            modifiers.push_back("COLOR_PROPERTY");
        } else if (prop_name.find("margin") != string::npos || prop_name.find("padding") != string::npos) {
            modifiers.push_back("SPACING_PROPERTY");
        } else if (prop_name.find("font") != string::npos || prop_name.find("text") != string::npos) {
            modifiers.push_back("TYPOGRAPHY_PROPERTY");
        } else if (prop_name.find("display") != string::npos || prop_name.find("position") != string::npos) {
            modifiers.push_back("LAYOUT_PROPERTY");
        } else if (prop_name.find("animation") != string::npos || prop_name.find("transition") != string::npos) {
            modifiers.push_back("ANIMATION_PROPERTY");
        }
        
        return modifiers;
    }
    
    static vector<string> ExtractNumberModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        
        // Check for units by looking at siblings
        TSNode parent = ts_node_parent(node);
        if (!ts_node_is_null(parent)) {
            uint32_t child_count = ts_node_child_count(parent);
            for (uint32_t i = 0; i < child_count; i++) {
                TSNode child = ts_node_child(parent, i);
                string child_type = ts_node_type(child);
                
                if (child_type == "unit") {
                    string unit = CSSNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS>::ExtractNodeText(child, content);
                    if (unit == "px" || unit == "em" || unit == "rem") {
                        modifiers.push_back("LENGTH_UNIT");
                    } else if (unit == "%" || unit == "vh" || unit == "vw") {
                        modifiers.push_back("RELATIVE_UNIT");
                    } else if (unit == "s" || unit == "ms") {
                        modifiers.push_back("TIME_UNIT");
                    }
                    break;
                }
            }
        }
        
        return modifiers;
    }
};

//==============================================================================
// CSS Class/Component Extraction
//==============================================================================

template<>
struct CSSNativeExtractor<NativeExtractionStrategy::CLASS_WITH_METHODS> {
    static NativeContext Extract(TSNode node, const string& content) {
        // For CSS, this could be used for component-like structures
        // Return empty context for now
        return NativeContext();
    }
};

//==============================================================================
// CSS Other Strategy Stubs
//==============================================================================

template<>
struct CSSNativeExtractor<NativeExtractionStrategy::ASYNC_FUNCTION> {
    static NativeContext Extract(TSNode node, const string& content) {
        return NativeContext();
    }
};

template<>
struct CSSNativeExtractor<NativeExtractionStrategy::ARROW_FUNCTION> {
    static NativeContext Extract(TSNode node, const string& content) {
        return NativeContext();
    }
};

template<>
struct CSSNativeExtractor<NativeExtractionStrategy::CLASS_WITH_INHERITANCE> {
    static NativeContext Extract(TSNode node, const string& content) {
        return NativeContext();
    }
};

template<>
struct CSSNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_DECORATORS> {
    static NativeContext Extract(TSNode node, const string& content) {
        return NativeContext();
    }
};

// Specialization for CUSTOM (CSS custom extraction)
template<>
struct CSSNativeExtractor<NativeExtractionStrategy::CUSTOM> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;
        context.signature_type = "css_custom";  // Placeholder
        return context;
    }
};

// Note: Template trait specialization for CSSAdapter is defined in native_context_extraction.hpp

} // namespace duckdb