#include "unified_ast_backend.hpp"
#include "unified_ast_backend_impl.hpp"
#include "language_adapter.hpp"
#include "semantic_types.hpp"
#include "ast_file_utils.hpp"
#include "ast_parsing_task.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/file_system.hpp"
#include "duckdb/common/vector_operations/generic_executor.hpp"
#include "duckdb/common/types/vector.hpp"
#include "duckdb/parallel/task_executor.hpp"
#include "duckdb/parallel/task_scheduler.hpp"
#include "utf8proc_wrapper.hpp"
#include <stack>

namespace duckdb {

// SanitizeUTF8 is defined in unified_ast_backend_impl.hpp

ASTResult UnifiedASTBackend::ParseToASTResult(const string& content, 
                                            const string& language, 
                                            const string& file_path,
                                            const ExtractionConfig& config) {
    
    // Phase 2: Use template-based parsing with ZERO virtual calls!
    auto& registry = LanguageAdapterRegistry::GetInstance();
    return registry.ParseContentTemplated(content, language, file_path, config);
}

// Legacy function for backward compatibility
ASTResult UnifiedASTBackend::ParseToASTResult(const string& content, 
                                            const string& language, 
                                            const string& file_path,
                                            int32_t peek_size,
                                            const string& peek_mode) {
    
    // Convert legacy parameters to ExtractionConfig
    ExtractionConfig config;
    config.peek_size = peek_size;
    
    // Map legacy peek_mode to PeekLevel
    if (peek_mode == "none") {
        config.peek = PeekLevel::NONE;
    } else if (peek_mode == "smart") {
        config.peek = PeekLevel::SMART;
    } else if (peek_mode == "full") {
        config.peek = PeekLevel::FULL;
    } else if (peek_mode == "compact") {
        config.peek = PeekLevel::SMART; // Map compact to smart
    } else {
        config.peek = PeekLevel::SMART; // Default
    }
    
    return ParseToASTResult(content, language, file_path, config);
}

void UnifiedASTBackend::PopulateSemanticFields(ASTNode& node, const LanguageAdapter* adapter, TSNode ts_node, const string& content) {
    // Get node configuration (virtual call)
    const NodeConfig* config = adapter->GetNodeConfig(node.type.raw);
    
    if (config) {
        // STRUCTURED FIELDS: Set semantic info in context
        node.context.normalized.semantic_type = config->semantic_type;
        node.context.normalized.universal_flags = config->flags;
        
        // Handle IS_KEYWORD_IF_LEAF: only apply IS_KEYWORD flag if node has no children
        if (config->flags & ASTNodeFlags::IS_KEYWORD_IF_LEAF) {
            // Remove the conditional flag
            node.context.normalized.universal_flags &= ~ASTNodeFlags::IS_KEYWORD_IF_LEAF;
            
            // Only add IS_KEYWORD if this is a leaf node (no children)
            if (ts_node_child_count(ts_node) == 0) {
                node.context.normalized.universal_flags |= ASTNodeFlags::IS_KEYWORD;
            }
        }
    } else {
        // Fallback: use PARSER_CONSTRUCT for unknown types
        node.context.normalized.semantic_type = SemanticTypes::PARSER_CONSTRUCT;
        node.context.normalized.universal_flags = 0;
    }
    
    // Set normalized type for display/compatibility
    node.type.normalized = SemanticTypes::GetSemanticTypeName(node.context.normalized.semantic_type);
    
    // Calculate arity binning
    node.context.normalized.arity_bin = ASTNode::BinArityFibonacci(ts_node_child_count(ts_node));
}

//==============================================================================
// Extraction Configuration Parsing
//==============================================================================

ExtractionConfig ParseExtractionConfig(const string& context_str,
                                     const string& source_str, 
                                     const string& structure_str,
                                     const string& peek_str,
                                     int32_t peek_size) {
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
    
    // Parse source level
    string source_lower = StringUtil::Lower(source_str);
    if (source_lower == "none") {
        config.source = SourceLevel::NONE;
    } else if (source_lower == "path") {
        config.source = SourceLevel::PATH;
    } else if (source_lower == "lines_only") {
        config.source = SourceLevel::LINES_ONLY;
    } else if (source_lower == "lines") {
        config.source = SourceLevel::LINES;
    } else if (source_lower == "full") {
        config.source = SourceLevel::FULL;
    } else {
        // Default to lines for invalid input
        config.source = SourceLevel::LINES;
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
    
    // Override peek_size if provided
    if (peek_size != 120) {
        config.peek_size = peek_size;
    }
    
    return config;
}


vector<LogicalType> UnifiedASTBackend::GetFlatTableSchema() {
    return {
        LogicalType::BIGINT,      // node_id
        LogicalType::VARCHAR,     // type
        LogicalType::VARCHAR,     // name
        LogicalType::VARCHAR,     // file_path
        LogicalType::VARCHAR,     // language
        LogicalType::UINTEGER,    // start_line
        LogicalType::UINTEGER,    // start_column
        LogicalType::UINTEGER,    // end_line
        LogicalType::UINTEGER,    // end_column
        LogicalType::BIGINT,      // parent_id (stays signed for -1)
        LogicalType::UINTEGER,    // depth
        LogicalType::UINTEGER,    // sibling_index
        LogicalType::UINTEGER,    // children_count
        LogicalType::UINTEGER,    // descendant_count
        LogicalType::VARCHAR,     // peek (source_text)
        // Semantic type fields
        LogicalType::UTINYINT,    // semantic_type
        LogicalType::UTINYINT     // flags (renamed from universal_flags, removed arity_bin)
    };
}

vector<string> UnifiedASTBackend::GetFlatTableColumnNames() {
    return {
        "node_id", "type", "name", "file_path", "language",
        "start_line", "start_column", "end_line", "end_column", 
        "parent_id", "depth", "sibling_index", "children_count", "descendant_count",
        "peek",
        // Semantic type fields  
        "semantic_type", "flags"
    };
}

LogicalType UnifiedASTBackend::GetASTStructSchema() {
    // Create the complete AST struct schema with taxonomy fields
    child_list_t<LogicalType> source_children;
    source_children.push_back(make_pair("file_path", LogicalType::VARCHAR));
    source_children.push_back(make_pair("language", LogicalType::VARCHAR));
    
    child_list_t<LogicalType> node_children;
    node_children.push_back(make_pair("node_id", LogicalType::BIGINT));
    node_children.push_back(make_pair("type", LogicalType::VARCHAR));
    node_children.push_back(make_pair("name", LogicalType::VARCHAR));
    node_children.push_back(make_pair("start_line", LogicalType::UINTEGER));
    node_children.push_back(make_pair("end_line", LogicalType::UINTEGER));
    node_children.push_back(make_pair("start_column", LogicalType::UINTEGER));
    node_children.push_back(make_pair("end_column", LogicalType::UINTEGER));
    node_children.push_back(make_pair("parent_id", LogicalType::BIGINT));
    node_children.push_back(make_pair("depth", LogicalType::UINTEGER));
    node_children.push_back(make_pair("sibling_index", LogicalType::UINTEGER));
    node_children.push_back(make_pair("children_count", LogicalType::UINTEGER));
    node_children.push_back(make_pair("descendant_count", LogicalType::UINTEGER));
    node_children.push_back(make_pair("peek", LogicalType::VARCHAR));
    // Semantic type fields
    node_children.push_back(make_pair("semantic_type", LogicalType::UTINYINT));
    node_children.push_back(make_pair("flags", LogicalType::UTINYINT));
    
    child_list_t<LogicalType> ast_children;
    ast_children.push_back(make_pair("nodes", LogicalType::LIST(LogicalType::STRUCT(node_children))));
    ast_children.push_back(make_pair("source", LogicalType::STRUCT(source_children)));
    
    return LogicalType::STRUCT(ast_children);
}

//==============================================================================
// NEW: Hierarchical Schema Functions for Structured Field Access
//==============================================================================

vector<LogicalType> UnifiedASTBackend::GetHierarchicalTableSchema() {
    // Source Location STRUCT
    child_list_t<LogicalType> source_children;
    source_children.push_back(make_pair("file_path", LogicalType::VARCHAR));
    source_children.push_back(make_pair("language", LogicalType::VARCHAR));
    source_children.push_back(make_pair("start_line", LogicalType::UINTEGER));
    source_children.push_back(make_pair("start_column", LogicalType::UINTEGER));
    source_children.push_back(make_pair("end_line", LogicalType::UINTEGER));
    source_children.push_back(make_pair("end_column", LogicalType::UINTEGER));
    
    // Tree Structure STRUCT
    child_list_t<LogicalType> structure_children;
    structure_children.push_back(make_pair("parent_id", LogicalType::BIGINT));
    structure_children.push_back(make_pair("depth", LogicalType::UINTEGER));
    structure_children.push_back(make_pair("sibling_index", LogicalType::UINTEGER));
    structure_children.push_back(make_pair("children_count", LogicalType::UINTEGER));
    structure_children.push_back(make_pair("descendant_count", LogicalType::UINTEGER));
    
    // Context Information STRUCT (including native context)
    child_list_t<LogicalType> context_children;
    context_children.push_back(make_pair("name", LogicalType::VARCHAR));
    context_children.push_back(make_pair("semantic_type", LogicalType::UTINYINT));
    context_children.push_back(make_pair("flags", LogicalType::UTINYINT));
    
    // Native context STRUCT
    child_list_t<LogicalType> native_children;
    native_children.push_back(make_pair("signature_type", LogicalType::VARCHAR));
    native_children.push_back(make_pair("parameters", LogicalType::LIST(LogicalType::STRUCT({
        {"name", LogicalType::VARCHAR},
        {"type", LogicalType::VARCHAR},
        {"default_value", LogicalType::VARCHAR},
        {"is_optional", LogicalType::BOOLEAN},
        {"is_variadic", LogicalType::BOOLEAN},
        {"annotations", LogicalType::VARCHAR}
    }))));
    native_children.push_back(make_pair("modifiers", LogicalType::LIST(LogicalType::VARCHAR)));
    native_children.push_back(make_pair("qualified_name", LogicalType::VARCHAR));
    native_children.push_back(make_pair("annotations", LogicalType::VARCHAR));
    
    context_children.push_back(make_pair("native", LogicalType::STRUCT(native_children)));
    
    return {
        LogicalType::BIGINT,                          // node_id
        LogicalType::VARCHAR,                         // type (moved to base level)
        LogicalType::STRUCT(source_children),         // source
        LogicalType::STRUCT(structure_children),      // structure  
        LogicalType::STRUCT(context_children),        // context
        LogicalType::VARCHAR                          // peek
    };
}

vector<string> UnifiedASTBackend::GetHierarchicalTableColumnNames() {
    return {
        "node_id",     // BIGINT
        "type",        // VARCHAR (moved to base level)
        "source",      // STRUCT(file_path, language, start_line, start_column, end_line, end_column)
        "structure",   // STRUCT(parent_id, depth, sibling_index, children_count, descendant_count)
        "context",     // STRUCT(name, semantic_type, flags, native)
        "peek"         // VARCHAR
    };
}

LogicalType UnifiedASTBackend::GetHierarchicalStructSchema() {
    // Create structured schema with organized field groups
    
    // Source Location group
    child_list_t<LogicalType> source_location_children;
    source_location_children.push_back(make_pair("file_path", LogicalType::VARCHAR));
    source_location_children.push_back(make_pair("language", LogicalType::VARCHAR));
    source_location_children.push_back(make_pair("start_line", LogicalType::UINTEGER));
    source_location_children.push_back(make_pair("start_column", LogicalType::UINTEGER));
    source_location_children.push_back(make_pair("end_line", LogicalType::UINTEGER));
    source_location_children.push_back(make_pair("end_column", LogicalType::UINTEGER));
    
    // Tree Structure group
    child_list_t<LogicalType> tree_structure_children;
    tree_structure_children.push_back(make_pair("parent_id", LogicalType::BIGINT));
    tree_structure_children.push_back(make_pair("depth", LogicalType::UINTEGER));
    tree_structure_children.push_back(make_pair("sibling_index", LogicalType::UINTEGER));
    tree_structure_children.push_back(make_pair("children_count", LogicalType::UINTEGER));
    tree_structure_children.push_back(make_pair("descendant_count", LogicalType::UINTEGER));
    
    // Context Information group (semantic information only)
    child_list_t<LogicalType> context_info_children;
    context_info_children.push_back(make_pair("name", LogicalType::VARCHAR));
    context_info_children.push_back(make_pair("semantic_type", LogicalType::UTINYINT));
    context_info_children.push_back(make_pair("flags", LogicalType::UTINYINT));
    
    // Node with hierarchical structure
    child_list_t<LogicalType> node_children;
    node_children.push_back(make_pair("node_id", LogicalType::BIGINT));
    node_children.push_back(make_pair("type", LogicalType::VARCHAR));
    node_children.push_back(make_pair("source", LogicalType::STRUCT(source_location_children)));
    node_children.push_back(make_pair("structure", LogicalType::STRUCT(tree_structure_children)));
    node_children.push_back(make_pair("context", LogicalType::STRUCT(context_info_children)));
    node_children.push_back(make_pair("peek", LogicalType::VARCHAR));
    
    // AST result with metadata
    child_list_t<LogicalType> metadata_children;
    metadata_children.push_back(make_pair("file_path", LogicalType::VARCHAR));
    metadata_children.push_back(make_pair("language", LogicalType::VARCHAR));
    
    child_list_t<LogicalType> ast_children;
    ast_children.push_back(make_pair("nodes", LogicalType::LIST(LogicalType::STRUCT(node_children))));
    ast_children.push_back(make_pair("metadata", LogicalType::STRUCT(metadata_children)));
    
    return LogicalType::STRUCT(ast_children);
}

void UnifiedASTBackend::ProjectToTable(const ASTResult& result, DataChunk& output, idx_t& current_row, idx_t& output_index) {
    // Verify output chunk has correct number of columns (17 with language, removed arity_bin)
    if (output.ColumnCount() != 17) {
        throw InternalException("Output chunk has " + to_string(output.ColumnCount()) + " columns, expected 17");
    }
    
    // Get output vectors
    auto node_id_vec = FlatVector::GetData<int64_t>(output.data[0]);
    auto type_vec = FlatVector::GetData<string_t>(output.data[1]);
    auto name_vec = FlatVector::GetData<string_t>(output.data[2]);
    auto file_path_vec = FlatVector::GetData<string_t>(output.data[3]);
    auto language_vec = FlatVector::GetData<string_t>(output.data[4]);
    auto start_line_vec = FlatVector::GetData<uint32_t>(output.data[5]);
    auto start_column_vec = FlatVector::GetData<uint32_t>(output.data[6]);
    auto end_line_vec = FlatVector::GetData<uint32_t>(output.data[7]);
    auto end_column_vec = FlatVector::GetData<uint32_t>(output.data[8]);
    auto parent_id_vec = FlatVector::GetData<int64_t>(output.data[9]);
    auto depth_vec = FlatVector::GetData<uint32_t>(output.data[10]);
    auto sibling_index_vec = FlatVector::GetData<uint32_t>(output.data[11]);
    auto children_count_vec = FlatVector::GetData<uint32_t>(output.data[12]);
    auto descendant_count_vec = FlatVector::GetData<uint32_t>(output.data[13]);
    auto peek_vec = FlatVector::GetData<string_t>(output.data[14]);
    // Semantic type fields
    auto semantic_type_vec = FlatVector::GetData<uint8_t>(output.data[15]);
    auto flags_vec = FlatVector::GetData<uint8_t>(output.data[16]);
    
    // Get validity masks for nullable fields
    auto &name_validity = FlatVector::Validity(output.data[2]);
    auto &parent_validity = FlatVector::Validity(output.data[9]);
    auto &peek_validity = FlatVector::Validity(output.data[14]);
    
    idx_t count = 0;
    idx_t max_count = STANDARD_VECTOR_SIZE;
    
    // Start from current_row and process up to STANDARD_VECTOR_SIZE rows
    for (idx_t i = current_row; i < result.nodes.size() && count < max_count; i++) {
        const auto& node = result.nodes[i];
        
        // Basic fields
        node_id_vec[output_index + count] = node.node_id;
        type_vec[output_index + count] = StringVector::AddString(output.data[1], node.type.raw);
        
        if (node.name.raw.empty()) {
            name_validity.SetInvalid(output_index + count);
        } else {
            name_vec[output_index + count] = StringVector::AddString(output.data[2], node.name.raw);
        }
        
        // Now that we process each file separately, each result has the correct file_path
        file_path_vec[output_index + count] = StringVector::AddString(output.data[3], result.source.file_path);
        language_vec[output_index + count] = StringVector::AddString(output.data[4], result.source.language);
        start_line_vec[output_index + count] = node.file_position.start_line;
        start_column_vec[output_index + count] = node.file_position.start_column;
        end_line_vec[output_index + count] = node.file_position.end_line;
        end_column_vec[output_index + count] = node.file_position.end_column;
        
        if (node.tree_position.parent_index < 0) {
            parent_validity.SetInvalid(output_index + count);
        } else {
            parent_id_vec[output_index + count] = node.tree_position.parent_index;
        }
        
        depth_vec[output_index + count] = node.tree_position.node_depth;
        sibling_index_vec[output_index + count] = node.tree_position.sibling_index;
        children_count_vec[output_index + count] = node.subtree.children_count;
        descendant_count_vec[output_index + count] = node.subtree.descendant_count;
        if (node.peek.empty()) {
            peek_validity.SetInvalid(output_index + count);
        } else {
            peek_vec[output_index + count] = StringVector::AddString(output.data[14], node.peek);
        }
        
        // Semantic type fields
        semantic_type_vec[output_index + count] = node.semantic_type;
        flags_vec[output_index + count] = node.universal_flags;
        
        count++;
        current_row++;  // Track which row we're on
    }
    
    output_index += count;
}

Value UnifiedASTBackend::CreateASTStruct(const ASTResult& result) {
    // Create source struct
    child_list_t<Value> source_children;
    source_children.push_back(make_pair("file_path", Value(result.source.file_path)));
    source_children.push_back(make_pair("language", Value(result.source.language)));
    Value source_value = Value::STRUCT(source_children);
    
    // Create nodes array
    vector<Value> node_values;
    node_values.reserve(result.nodes.size());
    
    for (const auto& node : result.nodes) {
        child_list_t<Value> node_children;
        node_children.push_back(make_pair("node_id", Value::BIGINT(node.node_id)));
        node_children.push_back(make_pair("type", Value(node.type.raw)));
        node_children.push_back(make_pair("name", Value(node.name.raw)));
        node_children.push_back(make_pair("start_line", Value::UINTEGER(node.file_position.start_line)));
        node_children.push_back(make_pair("end_line", Value::UINTEGER(node.file_position.end_line)));
        node_children.push_back(make_pair("start_column", Value::UINTEGER(node.file_position.start_column)));
        node_children.push_back(make_pair("end_column", Value::UINTEGER(node.file_position.end_column)));
        node_children.push_back(make_pair("parent_id", node.tree_position.parent_index >= 0 ? 
                                         Value::BIGINT(node.tree_position.parent_index) : Value()));
        node_children.push_back(make_pair("depth", Value::UINTEGER(node.tree_position.node_depth)));
        node_children.push_back(make_pair("sibling_index", Value::UINTEGER(node.tree_position.sibling_index)));
        node_children.push_back(make_pair("children_count", Value::UINTEGER(node.subtree.children_count)));
        node_children.push_back(make_pair("descendant_count", Value::UINTEGER(node.subtree.descendant_count)));
        node_children.push_back(make_pair("peek", Value(node.peek)));
        // Semantic type fields
        node_children.push_back(make_pair("semantic_type", Value::UTINYINT(node.semantic_type)));
        node_children.push_back(make_pair("flags", Value::UTINYINT(node.universal_flags)));
        
        node_values.push_back(Value::STRUCT(node_children));
    }
    
    // Create AST struct with proper node schema
    child_list_t<LogicalType> node_schema;
    node_schema.push_back(make_pair("node_id", LogicalType::BIGINT));
    node_schema.push_back(make_pair("type", LogicalType::VARCHAR));
    node_schema.push_back(make_pair("name", LogicalType::VARCHAR));
    node_schema.push_back(make_pair("start_line", LogicalType::UINTEGER));
    node_schema.push_back(make_pair("end_line", LogicalType::UINTEGER));
    node_schema.push_back(make_pair("start_column", LogicalType::UINTEGER));
    node_schema.push_back(make_pair("end_column", LogicalType::UINTEGER));
    node_schema.push_back(make_pair("parent_id", LogicalType::BIGINT));
    node_schema.push_back(make_pair("depth", LogicalType::UINTEGER));
    node_schema.push_back(make_pair("sibling_index", LogicalType::UINTEGER));
    node_schema.push_back(make_pair("children_count", LogicalType::UINTEGER));
    node_schema.push_back(make_pair("descendant_count", LogicalType::UINTEGER));
    node_schema.push_back(make_pair("peek", LogicalType::VARCHAR));
    node_schema.push_back(make_pair("semantic_type", LogicalType::UTINYINT));
    node_schema.push_back(make_pair("flags", LogicalType::UTINYINT));
    
    child_list_t<Value> ast_children;
    ast_children.push_back(make_pair("nodes", Value::LIST(LogicalType::STRUCT(node_schema), node_values)));
    ast_children.push_back(make_pair("source", source_value));
    
    return Value::STRUCT(ast_children);
}

Value UnifiedASTBackend::CreateASTStructValue(const ASTResult& result) {
    // Same as CreateASTStruct for now - both return a single struct value
    return CreateASTStruct(result);
}

//==============================================================================
// NEW: Hierarchical Table and Struct Functions
//==============================================================================

void UnifiedASTBackend::ProjectToHierarchicalTable(const ASTResult& result, DataChunk& output, idx_t& current_row, idx_t& output_index) {
    // Verify output chunk has correct number of columns (6 for hierarchical STRUCT schema)
    if (output.ColumnCount() != 6) {
        throw InternalException("Output chunk has " + to_string(output.ColumnCount()) + " columns, expected 6 for hierarchical STRUCT schema");
    }
    
    // Get output vectors for hierarchical schema: node_id, type, source, structure, context, peek
    auto node_id_vec = FlatVector::GetData<int64_t>(output.data[0]);
    auto type_vec = FlatVector::GetData<string_t>(output.data[1]);
    auto &source_vector = output.data[2];      // STRUCT column
    auto &structure_vector = output.data[3];   // STRUCT column  
    auto &context_vector = output.data[4];     // STRUCT column
    auto peek_vec = FlatVector::GetData<string_t>(output.data[5]);
    
    // Get validity masks
    auto &peek_validity = FlatVector::Validity(output.data[5]);
    
    idx_t count = 0;
    idx_t max_count = STANDARD_VECTOR_SIZE;
    
    // Process rows using NEW STRUCTURED FIELDS and create STRUCT values
    for (idx_t i = current_row; i < result.nodes.size() && count < max_count; i++) {
        const auto& node = result.nodes[i];
        idx_t row_idx = output_index + count;
        
        // Core identity
        node_id_vec[row_idx] = node.node_id;
        type_vec[row_idx] = StringVector::AddString(output.data[1], node.type.raw);
        
        // Create source STRUCT (use legacy fields for now)
        child_list_t<Value> source_values;
        source_values.push_back(make_pair("file_path", Value(result.source.file_path)));
        source_values.push_back(make_pair("language", Value(result.source.language)));
        source_values.push_back(make_pair("start_line", Value::UINTEGER(node.file_position.start_line)));
        source_values.push_back(make_pair("start_column", Value::UINTEGER(node.file_position.start_column)));
        source_values.push_back(make_pair("end_line", Value::UINTEGER(node.file_position.end_line)));
        source_values.push_back(make_pair("end_column", Value::UINTEGER(node.file_position.end_column)));
        Value source_struct = Value::STRUCT(source_values);
        FlatVector::GetData<Value>(source_vector)[row_idx] = source_struct;
        
        // Create structure STRUCT (use legacy fields for now)
        child_list_t<Value> structure_values;
        if (node.tree_position.parent_index < 0) {
            structure_values.push_back(make_pair("parent_id", Value()));  // NULL
        } else {
            structure_values.push_back(make_pair("parent_id", Value::BIGINT(node.tree_position.parent_index)));
        }
        structure_values.push_back(make_pair("depth", Value::UINTEGER(node.tree_position.node_depth)));
        structure_values.push_back(make_pair("sibling_index", Value::UINTEGER(node.tree_position.sibling_index)));
        structure_values.push_back(make_pair("children_count", Value::UINTEGER(node.subtree.children_count)));
        structure_values.push_back(make_pair("descendant_count", Value::UINTEGER(node.subtree.descendant_count)));
        Value structure_struct = Value::STRUCT(structure_values);
        FlatVector::GetData<Value>(structure_vector)[row_idx] = structure_struct;
        
        // Create context STRUCT (use legacy fields for now)
        child_list_t<Value> context_values;
        context_values.push_back(make_pair("type", Value(node.type.raw)));
        if (!node.name.raw.empty()) {
            context_values.push_back(make_pair("name", Value(node.name.raw)));
        } else {
            context_values.push_back(make_pair("name", Value()));  // NULL
        }
        context_values.push_back(make_pair("semantic_type", Value::UTINYINT(node.semantic_type)));
        context_values.push_back(make_pair("flags", Value::UTINYINT(node.universal_flags)));
        Value context_struct = Value::STRUCT(context_values);
        FlatVector::GetData<Value>(context_vector)[row_idx] = context_struct;
        
        // Content Preview 
        if (!node.peek.empty()) {
            peek_vec[row_idx] = StringVector::AddString(output.data[4], node.peek);
        } else {
            peek_validity.SetInvalid(row_idx);
        }
        
        count++;
        current_row++;
    }
    
    output_index += count;
}

void UnifiedASTBackend::ProjectToHierarchicalTableStreaming(const vector<ASTNode>& nodes, DataChunk& output, 
                                                          idx_t start_row, idx_t& output_index, 
                                                          const ASTSource& source_info) {
    // Verify output chunk has correct number of columns (6 for hierarchical STRUCT schema)
    if (output.ColumnCount() != 6) {
        throw InternalException("Output chunk has " + to_string(output.ColumnCount()) + " columns, expected 6 for hierarchical STRUCT schema");
    }
    
    // Get output vectors for new 6-column schema: node_id, type, source, structure, context, peek
    auto node_id_vec = FlatVector::GetData<int64_t>(output.data[0]);
    auto type_vec = FlatVector::GetData<string_t>(output.data[1]);
    auto peek_vec = FlatVector::GetData<string_t>(output.data[5]);
    auto &peek_validity = FlatVector::Validity(output.data[5]);
    
    // Get STRUCT child vectors using StructVector::GetEntries()
    auto &source_entries = StructVector::GetEntries(output.data[2]);
    auto &structure_entries = StructVector::GetEntries(output.data[3]);
    auto &context_entries = StructVector::GetEntries(output.data[4]);
    
    // Source STRUCT child vectors (file_path, language, start_line, start_column, end_line, end_column)
    auto source_file_path_vec = FlatVector::GetData<string_t>(*source_entries[0]);
    auto source_language_vec = FlatVector::GetData<string_t>(*source_entries[1]);
    auto source_start_line_vec = FlatVector::GetData<uint32_t>(*source_entries[2]);
    auto source_start_column_vec = FlatVector::GetData<uint32_t>(*source_entries[3]);
    auto source_end_line_vec = FlatVector::GetData<uint32_t>(*source_entries[4]);
    auto source_end_column_vec = FlatVector::GetData<uint32_t>(*source_entries[5]);
    
    // Structure STRUCT child vectors (parent_id, depth, sibling_index, children_count, descendant_count)
    auto structure_parent_id_vec = FlatVector::GetData<int64_t>(*structure_entries[0]);
    auto &structure_parent_validity = FlatVector::Validity(*structure_entries[0]);
    auto structure_depth_vec = FlatVector::GetData<uint32_t>(*structure_entries[1]);
    auto structure_sibling_index_vec = FlatVector::GetData<uint32_t>(*structure_entries[2]);
    auto structure_children_count_vec = FlatVector::GetData<uint32_t>(*structure_entries[3]);
    auto structure_descendant_count_vec = FlatVector::GetData<uint32_t>(*structure_entries[4]);
    
    // Context STRUCT child vectors (name, semantic_type, flags, native) - type moved to base level
    auto context_name_vec = FlatVector::GetData<string_t>(*context_entries[0]);
    auto &context_name_validity = FlatVector::Validity(*context_entries[0]);
    auto context_semantic_type_vec = FlatVector::GetData<uint8_t>(*context_entries[1]);
    auto context_flags_vec = FlatVector::GetData<uint8_t>(*context_entries[2]);
    
    // Native context STRUCT child vectors (4th field in context)
    auto &native_entries = StructVector::GetEntries(*context_entries[3]);
    auto &native_validity = FlatVector::Validity(*context_entries[3]);
    
    idx_t count = 0;
    idx_t max_count = STANDARD_VECTOR_SIZE - output_index;  // Account for already used rows
    
    // Process nodes starting from start_row
    for (idx_t i = start_row; i < nodes.size() && count < max_count; i++) {
        const auto& node = nodes[i];
        idx_t row_idx = output_index + count;
        
        // Core identity and type (moved to base level)
        node_id_vec[row_idx] = node.node_id;
        type_vec[row_idx] = StringVector::AddString(output.data[1], node.type.raw);
        
        // Populate source STRUCT fields
        source_file_path_vec[row_idx] = StringVector::AddString(*source_entries[0], source_info.file_path);
        source_language_vec[row_idx] = StringVector::AddString(*source_entries[1], source_info.language);
        source_start_line_vec[row_idx] = node.file_position.start_line;
        source_start_column_vec[row_idx] = node.file_position.start_column;
        source_end_line_vec[row_idx] = node.file_position.end_line;
        source_end_column_vec[row_idx] = node.file_position.end_column;
        
        // Populate structure STRUCT fields
        if (node.tree_position.parent_index < 0) {
            structure_parent_validity.SetInvalid(row_idx);
        } else {
            structure_parent_id_vec[row_idx] = node.tree_position.parent_index;
        }
        structure_depth_vec[row_idx] = node.tree_position.node_depth;
        structure_sibling_index_vec[row_idx] = node.tree_position.sibling_index;
        structure_children_count_vec[row_idx] = node.subtree.children_count;
        structure_descendant_count_vec[row_idx] = node.subtree.descendant_count;
        
        // Populate context STRUCT fields (no type field - moved to base level)
        if (!node.context.name.empty()) {
            context_name_vec[row_idx] = StringVector::AddString(*context_entries[0], node.context.name);
        } else {
            context_name_validity.SetInvalid(row_idx);
        }
        context_semantic_type_vec[row_idx] = node.context.normalized.semantic_type;
        context_flags_vec[row_idx] = node.context.normalized.universal_flags;
        
        // Populate native context STRUCT (4th field in context)
        if (node.context.native_extraction_attempted) {
            
            // Native context was extracted - populate the struct fields directly
            // Native struct has: signature_type, parameters, modifiers, qualified_name, annotations
            auto native_signature_type_vec = FlatVector::GetData<string_t>(*native_entries[0]);
            auto &native_signature_type_validity = FlatVector::Validity(*native_entries[0]);
            auto native_qualified_name_vec = FlatVector::GetData<string_t>(*native_entries[3]);
            auto &native_qualified_name_validity = FlatVector::Validity(*native_entries[3]);
            auto native_annotations_vec = FlatVector::GetData<string_t>(*native_entries[4]);
            auto &native_annotations_validity = FlatVector::Validity(*native_entries[4]);
            
            // Populate simple fields
            if (!node.context.native.signature_type.empty()) {
                native_signature_type_vec[row_idx] = StringVector::AddString(*native_entries[0], node.context.native.signature_type);
            } else {
                native_signature_type_validity.SetInvalid(row_idx);
            }
            
            if (!node.context.native.qualified_name.empty()) {
                native_qualified_name_vec[row_idx] = StringVector::AddString(*native_entries[3], node.context.native.qualified_name);
            } else {
                native_qualified_name_validity.SetInvalid(row_idx);
            }
            
            if (!node.context.native.annotations.empty()) {
                native_annotations_vec[row_idx] = StringVector::AddString(*native_entries[4], node.context.native.annotations);
            } else {
                native_annotations_validity.SetInvalid(row_idx);
            }
            
            // Handle modifiers list (native_entries[2])
            auto &modifiers_list_vector = *native_entries[2];
            auto modifiers_data = ListVector::GetData(modifiers_list_vector);
            auto &modifiers_validity = FlatVector::Validity(modifiers_list_vector);
            
            if (!node.context.native.modifiers.empty()) {
                // Set up the list entry for this row - get offset before adding elements
                auto list_offset = ListVector::GetListSize(modifiers_list_vector);
                modifiers_data[row_idx].offset = list_offset;
                modifiers_data[row_idx].length = node.context.native.modifiers.size();
                
                // Add each modifier to the child vector atomically
                for (const auto& modifier : node.context.native.modifiers) {
                    ListVector::PushBack(modifiers_list_vector, Value(modifier));
                }
            } else {
                // Empty list
                modifiers_data[row_idx].offset = ListVector::GetListSize(modifiers_list_vector);
                modifiers_data[row_idx].length = 0;
            }
            
            // Handle parameters list (native_entries[1]) - complex struct list
            auto &parameters_list_vector = *native_entries[1];
            auto parameters_data = ListVector::GetData(parameters_list_vector);
            auto &parameters_validity = FlatVector::Validity(parameters_list_vector);
            
            if (!node.context.native.parameters.empty()) {
                // Set up the list entry for this row
                auto list_offset = ListVector::GetListSize(parameters_list_vector);
                parameters_data[row_idx].offset = list_offset;
                parameters_data[row_idx].length = node.context.native.parameters.size();
                
                // Add each parameter as a struct to the child vector
                for (const auto& param : node.context.native.parameters) {
                    child_list_t<Value> param_struct;
                    param_struct.emplace_back("name", param.name.empty() ? Value(LogicalType::VARCHAR) : Value(param.name));
                    param_struct.emplace_back("type", param.type.empty() ? Value(LogicalType::VARCHAR) : Value(param.type));
                    param_struct.emplace_back("default_value", param.default_value.empty() ? Value(LogicalType::VARCHAR) : Value(param.default_value));
                    param_struct.emplace_back("is_optional", Value::BOOLEAN(param.is_optional));
                    param_struct.emplace_back("is_variadic", Value::BOOLEAN(param.is_variadic));
                    param_struct.emplace_back("annotations", param.annotations.empty() ? Value(LogicalType::VARCHAR) : Value(param.annotations));
                    
                    ListVector::PushBack(parameters_list_vector, Value::STRUCT(move(param_struct)));
                }
            } else {
                // Empty list
                parameters_data[row_idx].offset = ListVector::GetListSize(parameters_list_vector);
                parameters_data[row_idx].length = 0;
            }
            
        } else {
            // No extraction attempted - set native field to NULL
            native_validity.SetInvalid(row_idx);
        }
        
        // Content Preview 
        if (!node.peek.empty()) {
            peek_vec[row_idx] = StringVector::AddString(output.data[5], node.peek);
        } else {
            peek_validity.SetInvalid(row_idx);
        }
        
        count++;
    }
    
    output_index += count;
}

Value UnifiedASTBackend::CreateHierarchicalASTStruct(const ASTResult& result) {
    // Create metadata struct
    child_list_t<Value> metadata_children;
    metadata_children.push_back(make_pair("file_path", Value(result.source.file_path)));
    metadata_children.push_back(make_pair("language", Value(result.source.language)));
    Value metadata_value = Value::STRUCT(metadata_children);
    
    // Create nodes array with hierarchical structure
    vector<Value> node_values;
    node_values.reserve(result.nodes.size());
    
    for (const auto& node : result.nodes) {
        // Source Location struct - use legacy fields for now
        child_list_t<Value> source_children;
        source_children.push_back(make_pair("file_path", Value(result.source.file_path)));
        source_children.push_back(make_pair("language", Value(result.source.language)));
        source_children.push_back(make_pair("start_line", Value::UINTEGER(node.file_position.start_line)));
        source_children.push_back(make_pair("start_column", Value::UINTEGER(node.file_position.start_column)));
        source_children.push_back(make_pair("end_line", Value::UINTEGER(node.file_position.end_line)));
        source_children.push_back(make_pair("end_column", Value::UINTEGER(node.file_position.end_column)));
        Value source_value = Value::STRUCT(source_children);
        
        // Tree Structure struct - use legacy fields for now
        child_list_t<Value> structure_children;
        structure_children.push_back(make_pair("parent_id", node.tree_position.parent_index >= 0 ? 
                                             Value::BIGINT(node.tree_position.parent_index) : Value()));
        structure_children.push_back(make_pair("depth", Value::UINTEGER(node.tree_position.node_depth)));
        structure_children.push_back(make_pair("sibling_index", Value::UINTEGER(node.tree_position.sibling_index)));
        structure_children.push_back(make_pair("children_count", Value::UINTEGER(node.subtree.children_count)));
        structure_children.push_back(make_pair("descendant_count", Value::UINTEGER(node.subtree.descendant_count)));
        Value structure_value = Value::STRUCT(structure_children);
        
        // Context Information struct (semantic information only)
        child_list_t<Value> context_children;
        context_children.push_back(make_pair("name", Value(node.name.raw)));
        context_children.push_back(make_pair("semantic_type", Value::UTINYINT(node.semantic_type)));
        context_children.push_back(make_pair("flags", Value::UTINYINT(node.universal_flags)));
        Value context_value = Value::STRUCT(context_children);
        
        // Complete node struct with type at base level
        child_list_t<Value> node_children;
        node_children.push_back(make_pair("node_id", Value::BIGINT(node.node_id)));
        node_children.push_back(make_pair("type", Value(node.type.raw)));
        node_children.push_back(make_pair("source", source_value));
        node_children.push_back(make_pair("structure", structure_value));
        node_children.push_back(make_pair("context", context_value));
        node_children.push_back(make_pair("peek", Value(node.peek)));
        
        node_values.push_back(Value::STRUCT(node_children));
    }
    
    // Create hierarchical AST struct with proper schema
    // Get the node schema for the list
    child_list_t<LogicalType> source_location_children;
    source_location_children.push_back(make_pair("file_path", LogicalType::VARCHAR));
    source_location_children.push_back(make_pair("language", LogicalType::VARCHAR));
    source_location_children.push_back(make_pair("start_line", LogicalType::UINTEGER));
    source_location_children.push_back(make_pair("start_column", LogicalType::UINTEGER));
    source_location_children.push_back(make_pair("end_line", LogicalType::UINTEGER));
    source_location_children.push_back(make_pair("end_column", LogicalType::UINTEGER));
    
    child_list_t<LogicalType> tree_structure_children;
    tree_structure_children.push_back(make_pair("parent_id", LogicalType::BIGINT));
    tree_structure_children.push_back(make_pair("depth", LogicalType::UINTEGER));
    tree_structure_children.push_back(make_pair("sibling_index", LogicalType::UINTEGER));
    tree_structure_children.push_back(make_pair("children_count", LogicalType::UINTEGER));
    tree_structure_children.push_back(make_pair("descendant_count", LogicalType::UINTEGER));
    
    child_list_t<LogicalType> context_info_children;
    context_info_children.push_back(make_pair("type", LogicalType::VARCHAR));
    context_info_children.push_back(make_pair("name", LogicalType::VARCHAR));
    context_info_children.push_back(make_pair("semantic_type", LogicalType::UTINYINT));
    context_info_children.push_back(make_pair("flags", LogicalType::UTINYINT));
    
    child_list_t<LogicalType> node_children;
    node_children.push_back(make_pair("node_id", LogicalType::BIGINT));
    node_children.push_back(make_pair("type", LogicalType::VARCHAR));
    node_children.push_back(make_pair("source", LogicalType::STRUCT(source_location_children)));
    node_children.push_back(make_pair("structure", LogicalType::STRUCT(tree_structure_children)));
    node_children.push_back(make_pair("context", LogicalType::STRUCT(context_info_children)));
    node_children.push_back(make_pair("peek", LogicalType::VARCHAR));
    
    child_list_t<Value> ast_children;
    ast_children.push_back(make_pair("nodes", Value::LIST(LogicalType::STRUCT(node_children), node_values)));
    ast_children.push_back(make_pair("metadata", metadata_value));
    
    return Value::STRUCT(ast_children);
}

ASTResultCollection UnifiedASTBackend::ParseFilesToASTCollection(ClientContext &context,
                                                               const Value &file_path_value,
                                                               const string& language,
                                                               bool ignore_errors,
                                                               int32_t peek_size,
                                                               const string& peek_mode) {
    // Get all files that match the input pattern(s)
    vector<string> supported_extensions;
    if (language != "auto") {
        supported_extensions = ASTFileUtils::GetSupportedExtensions(language);
    }
    
    auto file_paths = ASTFileUtils::GetFiles(context, file_path_value, ignore_errors, supported_extensions);
    
    if (file_paths.empty() && !ignore_errors) {
        throw IOException("No files found matching the input pattern");
    }
    
    // Parse all files as separate results
    ASTResultCollection collection;
    
    for (const auto& file_path : file_paths) {
        try {
            // Auto-detect language if needed
            string file_language = language;
            if (language == "auto" || language.empty()) {
                file_language = ASTFileUtils::DetectLanguageFromPath(file_path);
                if (file_language == "auto") {
                    if (!ignore_errors) {
                        throw BinderException("Could not detect language for file: " + file_path);
                    }
                    continue; // Skip this file
                }
            }
            
            // Read file content
            auto &fs = FileSystem::GetFileSystem(context);
            if (!fs.FileExists(file_path)) {
                if (!ignore_errors) {
                    throw IOException("File does not exist: " + file_path);
                }
                continue; // Skip missing files
            }
            
            auto handle = fs.OpenFile(file_path, FileFlags::FILE_FLAGS_READ);
            auto file_size = fs.GetFileSize(*handle);
            
            string content;
            content.resize(file_size);
            fs.Read(*handle, (void*)content.data(), file_size);
            
            // Parse this file as a separate result
            auto file_result = ParseToASTResult(content, file_language, file_path, peek_size, peek_mode);
            
            // Add this individual result to the collection
            collection.results.push_back(std::move(file_result));
            
        } catch (const Exception &e) {
            if (!ignore_errors) {
                throw IOException("Failed to parse file '" + file_path + "': " + string(e.what()));
            }
            // With ignore_errors=true, continue processing other files
        }
    }
    
    return collection;
}

unique_ptr<ASTResult> UnifiedASTBackend::ParseSingleFileToASTResult(ClientContext &context,
                                                                   const string& file_path,
                                                                   const string& language,
                                                                   bool ignore_errors,
                                                                   int32_t peek_size,
                                                                   const string& peek_mode) {
    try {
        // Auto-detect language if needed
        string file_language = language;
        if (language == "auto") {
            file_language = ASTFileUtils::DetectLanguageFromPath(file_path);
            if (file_language == "auto") {
                if (!ignore_errors) {
                    throw BinderException("Could not detect language for file: " + file_path);
                }
                return nullptr; // Skip this file
            }
        }
        
        // Read file content
        auto &fs = FileSystem::GetFileSystem(context);
        if (!fs.FileExists(file_path)) {
            if (!ignore_errors) {
                throw IOException("File does not exist: " + file_path);
            }
            return nullptr; // Skip missing files
        }
        
        auto handle = fs.OpenFile(file_path, FileFlags::FILE_FLAGS_READ);
        auto file_size = fs.GetFileSize(*handle);
        
        string content;
        content.resize(file_size);
        fs.Read(*handle, (void*)content.data(), file_size);
        
        // Parse this file
        auto result = make_uniq<ASTResult>(ParseToASTResult(content, file_language, file_path, peek_size, peek_mode));
        return result;
        
    } catch (const Exception &e) {
        if (!ignore_errors) {
            throw IOException("Failed to parse file '" + file_path + "': " + string(e.what()));
        }
        return nullptr; // With ignore_errors=true, skip this file
    }
}

ASTResultCollection UnifiedASTBackend::ParseFilesToASTCollectionParallel(ClientContext &context,
                                                                        const Value &file_path_value,
                                                                        const string& language,
                                                                        bool ignore_errors,
                                                                        int32_t peek_size,
                                                                        const string& peek_mode) {
    // Get all files that match the input pattern(s)
    vector<string> supported_extensions;
    if (language != "auto") {
        supported_extensions = ASTFileUtils::GetSupportedExtensions(language);
    }
    
    auto file_paths = ASTFileUtils::GetFiles(context, file_path_value, ignore_errors, supported_extensions);
    
    if (file_paths.empty() && !ignore_errors) {
        throw IOException("No files found matching the input pattern");
    }
    
    // For small file counts, use sequential processing to avoid task overhead
    if (file_paths.size() <= 1) {
        return ParseFilesToASTCollection(context, file_path_value, language, ignore_errors, peek_size, peek_mode);
    }
    
    // Determine thread count and task distribution
    const auto num_threads = NumericCast<idx_t>(TaskScheduler::GetScheduler(context).NumberOfThreads());
    const auto files_per_task = MaxValue<idx_t>((file_paths.size() + num_threads - 1) / num_threads, 1);
    const auto num_tasks = (file_paths.size() + files_per_task - 1) / files_per_task;
    
    // Auto-detect language for each file if needed
    vector<string> resolved_languages;
    resolved_languages.reserve(file_paths.size());
    
    for (const auto& file_path : file_paths) {
        string file_language = language;
        if (language == "auto" || language.empty()) {
            file_language = ASTFileUtils::DetectLanguageFromPath(file_path);
            if (file_language == "auto") {
                if (!ignore_errors) {
                    throw BinderException("Could not detect language for file: " + file_path);
                }
                file_language = "unknown"; // Will be skipped during processing
            }
        }
        resolved_languages.push_back(file_language);
    }
    
    // Create shared parsing state with pre-created adapters
    // For the unified backend, we'll use the traditional singleton approach for now
    // since this code path is less performance-critical than the streaming function
    unordered_map<string, unique_ptr<LanguageAdapter>> empty_adapters_map;
    ASTParsingState parsing_state(context, file_paths, resolved_languages, ignore_errors, peek_size, peek_mode, empty_adapters_map);
    
    // Create and schedule tasks
    TaskExecutor executor(context);
    vector<unique_ptr<ASTParsingTask>> tasks;
    tasks.reserve(num_tasks);
    
    for (idx_t task_idx = 0; task_idx < num_tasks; task_idx++) {
        const auto file_idx_start = task_idx * files_per_task;
        const auto file_idx_end = MinValue<idx_t>(file_idx_start + files_per_task, file_paths.size());
        
        auto task = make_uniq<ASTParsingTask>(executor, parsing_state, file_idx_start, file_idx_end, task_idx);
        executor.ScheduleTask(std::move(task));
    }
    
    // Execute all tasks and wait for completion
    executor.WorkOnTasks();
    
    // Report errors if any occurred (when ignore_errors=true)
    if (parsing_state.errors_encountered.load() > 0 && ignore_errors) {
        // Could log warnings about skipped files here
        // For now, silently continue as per ignore_errors=true behavior
    }
    
    // Collect results
    ASTResultCollection collection;
    collection.results = std::move(parsing_state.results);
    return collection;
}

} // namespace duckdb
