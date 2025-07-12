#include "structured_ast_types.hpp"
#include "ast_type.hpp"
#include "unified_ast_backend.hpp"
#include "duckdb/common/string_util.hpp"

namespace duckdb {

//==============================================================================
// Conversion Functions Implementation - UPDATED FOR FLAT STRUCTURE
//==============================================================================

void StructuredASTNode::populate_from_legacy(const ASTNode& legacy_node,
                                            ContextLevel context_level,
                                            LocationLevel location_level,
                                            StructureLevel structure_level) {
    // Core fields
    node_id = legacy_node.node_id;
    type_raw = legacy_node.type.raw;
    
    // Source location (based on location_level) - NOW USING FLAT FIELDS
    if (location_level >= LocationLevel::INPUT_ONLY) {
        // File path and language would come from ASTResult metadata
        // For now, leave empty - will be populated by caller
    }
    if (location_level >= LocationLevel::LINES) {
        // Use flat fields instead of legacy_node.file_position.field
        source.start_line = legacy_node.start_line;
        source.end_line = legacy_node.end_line;
    }
    if (location_level >= LocationLevel::FULL) {
        // Use flat fields instead of legacy_node.file_position.field
        source.start_column = legacy_node.start_column;
        source.end_column = legacy_node.end_column;
    }
    
    // Tree structure (based on structure_level) - NOW USING FLAT FIELDS
    if (structure_level >= StructureLevel::MINIMAL) {
        // Use flat fields instead of legacy_node.structure.field
        structure.parent_id = legacy_node.parent_id;
        structure.depth = legacy_node.depth;
        structure.sibling_index = legacy_node.sibling_index;
    }
    if (structure_level >= StructureLevel::FULL) {
        // Use flat fields instead of legacy_node.structure.field
        structure.children_count = legacy_node.children_count;
        structure.descendant_count = legacy_node.descendant_count;
    }
    
    // Context information (based on context_level)
    if (context_level >= ContextLevel::NODE_TYPES_ONLY) {
        context.semantic_type = legacy_node.semantic_type;
        context.universal_flags = legacy_node.universal_flags;
        context.arity_bin = legacy_node.arity_bin;
    }
    if (context_level >= ContextLevel::NORMALIZED) {
        context.name = legacy_node.name.raw;
    }
    // Native context would be handled separately if needed
}

void ASTNode::populate_from_structured(const StructuredASTNode& structured_node) {
    // Core fields
    node_id = structured_node.node_id;
    type.raw = structured_node.type_raw;
    
    // FLAT STRUCTURE POPULATION - Copy directly to flat fields
    // Source location - copy from structured to flat legacy fields
    start_line = structured_node.source.start_line;
    end_line = structured_node.source.end_line;
    start_column = static_cast<uint16_t>(structured_node.source.start_column);
    end_column = static_cast<uint16_t>(structured_node.source.end_column);
    
    // Tree structure - copy from structured to flat fields
    parent_id = structured_node.structure.parent_id;
    depth = structured_node.structure.depth;
    sibling_index = structured_node.structure.sibling_index;
    children_count = structured_node.structure.children_count;
    descendant_count = structured_node.structure.descendant_count;
    
    // Also populate legacy flat fields for compatibility
    parent_index = parent_id;
    node_depth = static_cast<uint8_t>(depth);
    legacy_sibling_index = sibling_index;
    legacy_children_count = static_cast<uint16_t>(children_count);
    legacy_descendant_count = static_cast<uint16_t>(descendant_count);
    node_index = static_cast<int64_t>(node_id);
    
    // Context/semantic info
    semantic_type = structured_node.context.semantic_type;
    universal_flags = structured_node.context.universal_flags;
    arity_bin = structured_node.context.arity_bin;
    name.raw = structured_node.context.name;
    
    // Update legacy computed fields
    UpdateComputedLegacyFields();
}

} // namespace duckdb