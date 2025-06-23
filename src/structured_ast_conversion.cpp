#include "structured_ast_types.hpp"
#include "ast_type.hpp"
#include "unified_ast_backend.hpp"
#include "duckdb/common/string_util.hpp"

namespace duckdb {

//==============================================================================
// Conversion Functions Implementation
//==============================================================================

void StructuredASTNode::populate_from_legacy(const ASTNode& legacy_node,
                                            ContextLevel context_level,
                                            LocationLevel location_level,
                                            StructureLevel structure_level) {
    // Core fields
    node_id = legacy_node.node_id;
    type_raw = legacy_node.type.raw;
    
    // Source location (based on location_level)
    if (location_level >= LocationLevel::INPUT_ONLY) {
        // File path and language would come from ASTResult metadata
        // For now, leave empty - will be populated by caller
    }
    if (location_level >= LocationLevel::LINES) {
        source.start_line = legacy_node.file_position.start_line;
        source.end_line = legacy_node.file_position.end_line;
    }
    if (location_level >= LocationLevel::FULL) {
        source.start_column = legacy_node.file_position.start_column;
        source.end_column = legacy_node.file_position.end_column;
    }
    
    // Tree structure (based on structure_level)
    if (structure_level >= StructureLevel::MINIMAL) {
        structure.parent_id = legacy_node.tree_position.parent_index;
        structure.depth = legacy_node.tree_position.node_depth;
        structure.sibling_index = legacy_node.tree_position.sibling_index;
    }
    if (structure_level >= StructureLevel::FULL) {
        structure.children_count = legacy_node.subtree.children_count;
        structure.descendant_count = legacy_node.subtree.descendant_count;
    }
    
    // Context information (based on context_level)
    if (context_level >= ContextLevel::NODE_TYPES_ONLY) {
        context.normalized.semantic_type = legacy_node.semantic_type;
        context.normalized.universal_flags = legacy_node.universal_flags;
        context.normalized.arity_bin = legacy_node.arity_bin;
    }
    if (context_level >= ContextLevel::NORMALIZED) {
        context.name = legacy_node.name.raw;
    }
    if (context_level >= ContextLevel::NATIVE) {
        // Native context fields would be populated by language-specific extraction
        // For now, leave empty - will be implemented in future phases
    }
    
    // Peek content (always available in legacy nodes)
    peek = legacy_node.peek;
}

StructuredASTResult ConvertToStructuredResult(const ASTResult& legacy_result,
                                            const ExtractionConfig& config) {
    StructuredASTResult structured_result;
    
    // Copy metadata
    structured_result.node_count = legacy_result.node_count;
    structured_result.max_depth = legacy_result.max_depth;
    
    // Set extraction configuration
    structured_result.context_level = config.context;
    structured_result.location_level = config.location;
    structured_result.structure_level = config.structure;
    structured_result.peek_level = config.peek;
    structured_result.peek_size = config.peek_size;
    
    // Populate source info
    if (config.location >= LocationLevel::INPUT_ONLY) {
        structured_result.source_info.file_path = legacy_result.source.file_path;
        structured_result.source_info.language = legacy_result.source.language;
    }
    
    // Convert all nodes
    structured_result.nodes.reserve(legacy_result.nodes.size());
    for (const auto& legacy_node : legacy_result.nodes) {
        StructuredASTNode structured_node;
        structured_node.populate_from_legacy(legacy_node, config.context, 
                                           config.location, config.structure);
        
        // Populate source info for each node
        if (config.location >= LocationLevel::INPUT_ONLY) {
            structured_node.source.file_path = legacy_result.source.file_path;
            structured_node.source.language = legacy_result.source.language;
        }
        
        structured_result.nodes.push_back(structured_node);
    }
    
    return structured_result;
}

ASTResult ConvertToLegacyResult(const StructuredASTResult& structured_result) {
    ASTResult legacy_result;
    
    // Copy metadata
    legacy_result.node_count = structured_result.node_count;
    legacy_result.max_depth = structured_result.max_depth;
    
    // Set source info
    legacy_result.source.file_path = structured_result.source_info.file_path;
    legacy_result.source.language = structured_result.source_info.language;
    
    // Convert all nodes back to legacy format
    legacy_result.nodes.reserve(structured_result.nodes.size());
    for (const auto& structured_node : structured_result.nodes) {
        ASTNode legacy_node;
        
        // Core fields
        legacy_node.node_id = structured_node.node_id;
        legacy_node.type.raw = structured_node.type_raw;
        
        // File position
        legacy_node.file_position.start_line = structured_node.source.start_line;
        legacy_node.file_position.end_line = structured_node.source.end_line;
        legacy_node.file_position.start_column = structured_node.source.start_column;
        legacy_node.file_position.end_column = structured_node.source.end_column;
        
        // Tree position
        legacy_node.tree_position.parent_index = structured_node.structure.parent_id;
        legacy_node.tree_position.node_depth = structured_node.structure.depth;
        legacy_node.tree_position.sibling_index = structured_node.structure.sibling_index;
        legacy_node.tree_position.node_index = structured_node.node_id;
        
        // Subtree info
        legacy_node.subtree.children_count = structured_node.structure.children_count;
        legacy_node.subtree.descendant_count = structured_node.structure.descendant_count;
        
        // Context/semantic info
        legacy_node.semantic_type = structured_node.context.normalized.semantic_type;
        legacy_node.universal_flags = structured_node.context.normalized.universal_flags;
        legacy_node.arity_bin = structured_node.context.normalized.arity_bin;
        legacy_node.name.raw = structured_node.context.name;
        
        // Update legacy computed fields
        legacy_node.UpdateLegacyFields();
        
        // Peek content
        legacy_node.peek = structured_node.peek;
        
        legacy_result.nodes.push_back(legacy_node);
    }
    
    return legacy_result;
}

ExtractionConfig ParseExtractionConfig(const string& context_str,
                                     const string& location_str,
                                     const string& structure_str,
                                     const string& peek_str) {
    ExtractionConfig config;
    
    // Parse context level
    string context_lower = StringUtil::Lower(context_str);
    if (context_lower == "none") {
        config.context = ContextLevel::NONE;
    } else if (context_lower == "node_types_only") {
        config.context = ContextLevel::NODE_TYPES_ONLY;
    } else if (context_lower == "normalized") {
        config.context = ContextLevel::NORMALIZED;
    } else if (context_lower == "native") {
        config.context = ContextLevel::NATIVE;
    } else {
        // Default to normalized for invalid input
        config.context = ContextLevel::NORMALIZED;
    }
    
    // Parse location level
    string location_lower = StringUtil::Lower(location_str);
    if (location_lower == "none") {
        config.location = LocationLevel::NONE;
    } else if (location_lower == "input_only") {
        config.location = LocationLevel::INPUT_ONLY;
    } else if (location_lower == "lines") {
        config.location = LocationLevel::LINES;
    } else if (location_lower == "full") {
        config.location = LocationLevel::FULL;
    } else {
        // Default to lines for invalid input
        config.location = LocationLevel::LINES;
    }
    
    // Parse structure level
    string structure_lower = StringUtil::Lower(structure_str);
    if (structure_lower == "none") {
        config.structure = StructureLevel::NONE;
    } else if (structure_lower == "minimal") {
        config.structure = StructureLevel::MINIMAL;
    } else if (structure_lower == "full") {
        config.structure = StructureLevel::FULL;
    } else {
        // Default to full for invalid input
        config.structure = StructureLevel::FULL;
    }
    
    // Parse peek level  
    string peek_lower = StringUtil::Lower(peek_str);
    if (peek_lower == "none") {
        config.peek = PeekLevel::NONE;
    } else if (peek_lower == "smart") {
        config.peek = PeekLevel::SMART;
    } else if (peek_lower == "full") {
        config.peek = PeekLevel::FULL;
    } else {
        // Try to parse as integer for custom size
        try {
            int32_t size = std::stoi(peek_str);
            if (size >= 0) {
                config.peek = PeekLevel::CUSTOM;
                config.peek_size = size;
            } else {
                // Default to smart for negative values
                config.peek = PeekLevel::SMART;
            }
        } catch (...) {
            // Default to smart for invalid input
            config.peek = PeekLevel::SMART;
        }
    }
    
    return config;
}

} // namespace duckdb