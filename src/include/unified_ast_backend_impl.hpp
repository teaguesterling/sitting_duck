#pragma once

#include "unified_ast_backend.hpp"
#include "native_context_extraction.hpp"
#include "python_native_extractors.hpp"
#include "javascript_native_extractors.hpp"
#include "typescript_native_extractors.hpp"
#include "java_native_extractors.hpp"
#include "cpp_native_extractors.hpp"
#include "rust_native_extractors.hpp"
#include "go_native_extractors.hpp"
#include "c_native_extractors.hpp"
#include "php_native_extractors.hpp"
#include "ruby_native_extractors.hpp"
#include "swift_native_extractors.hpp"
#include "kotlin_native_extractors.hpp"
#include "duckdb/common/string_util.hpp"
#include "utf8proc_wrapper.hpp"
#include <algorithm>

namespace duckdb {

// Helper function to ensure UTF-8 validity using DuckDB's UTF8 processor
static string SanitizeUTF8(const string& input) {
    if (input.empty()) {
        return input;
    }
    
    // Check if already valid
    if (Utf8Proc::IsValid(input.c_str(), input.size())) {
        return input;
    }
    
    // Create mutable char array for MakeValid (following DuckDB pattern)
    std::vector<char> char_array(input.begin(), input.end());
    char_array.push_back('\0'); // Null-terminate
    Utf8Proc::MakeValid(&char_array[0], char_array.size() - 1, '?');  // Replace invalid bytes with '?'
    return string(char_array.begin(), char_array.end() - 1);  // Exclude null terminator
}

// Template implementation with ExtractionConfig - eliminates virtual calls
template<typename AdapterType>
ASTResult UnifiedASTBackend::ParseToASTResultTemplated(const AdapterType* adapter,
                                                       const string& content, 
                                                       const string& language, 
                                                       const string& file_path,
                                                       const ExtractionConfig& config) {
    ASTResult result;
    result.source.file_path = file_path;
    result.source.language = language;
    auto start_time = std::chrono::system_clock::now();
    
    // Parse the content using the adapter's smart pointer wrapper
    TSTreePtr tree = adapter->ParseContent(content);
    if (!tree) {
        throw InternalException("Failed to parse content");
    }
    
    // Process tree into ASTNodes using DFS ordering with O(1) descendant counting
    TSNode root = ts_tree_root_node(tree.get());
    uint32_t max_depth = 0;
    
    struct StackEntry {
        TSNode node;
        int64_t parent_id;
        uint32_t depth;
        uint32_t sibling_index;
        bool processed;        // Track if node has been processed
        idx_t node_index;      // Index in nodes array for this node
    };
    
    // Hoist GetNodeConfigs outside the hot loop - huge performance optimization!
    const auto& node_configs = adapter->GetNodeConfigs();
    
    vector<StackEntry> stack;
    stack.push_back({root, -1, 0, 0, false, 0});
    
    while (!stack.empty()) {
        // Check if the top entry is processed before copying
        if (!stack.back().processed) {
            // Copy the entry to avoid reference invalidation when stack reallocates
            auto entry = stack.back();
            // First visit - create node and add children
            entry.processed = true;
            entry.node_index = result.nodes.size();
            
            // Update the stack entry with the processed flag and node_index
            stack.back().processed = true;
            stack.back().node_index = entry.node_index;
            
            // Track max depth
            max_depth = std::max(max_depth, entry.depth);
            
            // Create ASTNode
            ASTNode ast_node;
            
            // Basic information - use node_index as node_id
            ast_node.node_id = entry.node_index;
            ast_node.type.raw = ts_node_type(entry.node);
            
            // Position information -> NEW STRUCTURED FIELDS
            TSPoint start = ts_node_start_point(entry.node);
            TSPoint end = ts_node_end_point(entry.node);
            ast_node.source.start_line = start.row + 1;
            ast_node.source.end_line = end.row + 1;
            ast_node.source.start_column = start.column + 1;
            ast_node.source.end_column = end.column + 1;
            
            // Tree structure -> NEW STRUCTURED FIELDS
            ast_node.structure.parent_id = entry.parent_id;
            ast_node.structure.depth = entry.depth;
            ast_node.structure.sibling_index = entry.sibling_index;
            uint32_t child_count = ts_node_child_count(entry.node);
            ast_node.structure.children_count = child_count;
            ast_node.structure.descendant_count = 0; // Will be calculated on second visit
            
            // Context information -> NEW STRUCTURED FIELDS
            string raw_name = adapter->ExtractNodeName(entry.node, content);
            ast_node.context.name = SanitizeUTF8(raw_name);
            
            // Extract source text (peek) with configurable size and mode
            uint32_t start_byte = ts_node_start_byte(entry.node);
            uint32_t end_byte = ts_node_end_byte(entry.node);
            if (start_byte < content.size() && end_byte <= content.size() && end_byte > start_byte) {
                string source_text = content.substr(start_byte, end_byte - start_byte);
                
                // Apply peek configuration and sanitize UTF-8
                if (config.peek == PeekLevel::NONE || config.peek_size == 0) {
                    ast_node.peek = "";  // Empty string will become NULL in output
                } else if (config.peek == PeekLevel::FULL || config.peek_size == -1) {
                    ast_node.peek = SanitizeUTF8(source_text);
                } else if (config.peek == PeekLevel::SMART) {
                    // Smart mode: adapt to content size and type
                    if (source_text.length() <= 50) {
                        // Small nodes: full content
                        ast_node.peek = SanitizeUTF8(source_text);
                    } else if (source_text.find('\n') == string::npos) {
                        // Single-line: truncate at display width
                        ast_node.peek = SanitizeUTF8(source_text.length() > 80 ? source_text.substr(0, 77) + "..." : source_text);
                    } else {
                        // Multi-line: first meaningful line with smart truncation
                        size_t newline_pos = source_text.find('\n');
                        string first_line = source_text.substr(0, newline_pos);
                        ast_node.peek = SanitizeUTF8(first_line.length() > 80 ? first_line.substr(0, 77) + "..." : first_line);
                    }
                } else if (config.peek == PeekLevel::CUSTOM) {
                    // Custom size mode
                    uint32_t effective_size = config.peek_size > 0 ? config.peek_size : 120;
                    ast_node.peek = SanitizeUTF8(source_text.length() > effective_size ? source_text.substr(0, effective_size) : source_text);
                } else {
                    // Default fallback (shouldn't happen)
                    ast_node.peek = SanitizeUTF8(source_text.length() > 120 ? source_text.substr(0, 120) : source_text);
                }
            }
            
            // Populate semantic type and other fields - pass configs to avoid virtual call
            PopulateSemanticFieldsTemplated(ast_node, adapter, entry.node, content, node_configs);
            
            // Update legacy fields for backward compatibility
            ast_node.UpdateComputedLegacyFields();
            
            result.nodes.push_back(ast_node);
            
            // Add children to stack in reverse order for correct processing
            int64_t current_id = ast_node.tree_position.node_index;
            for (int32_t i = child_count - 1; i >= 0; i--) {
                TSNode child = ts_node_child(entry.node, i);
                stack.push_back({child, current_id, entry.depth + 1, static_cast<uint32_t>(i), false, 0});
            }
        } else {
            // Second visit - get the processed entry for descendant calculation
            auto entry = stack.back();
            
            // O(1) descendant count calculation!
            // All nodes between entry.node_index+1 and nodes.size() are descendants
            // due to DFS ordering
            int32_t descendant_count = result.nodes.size() - entry.node_index - 1;
            result.nodes[entry.node_index].structure.descendant_count = descendant_count;
            
            // Update legacy fields after descendant count change
            result.nodes[entry.node_index].UpdateComputedLegacyFields();
            
            stack.pop_back();
        }
    }
    
    // Smart pointer automatically cleans up the tree
    
    // Set metadata
    result.parse_time = start_time;
    result.node_count = result.nodes.size();
    result.max_depth = max_depth;
    
    return result;
}

// Templated version of PopulateSemanticFields - zero virtual calls!
template<typename AdapterType>
void PopulateSemanticFieldsTemplated(ASTNode& node, const AdapterType* adapter, TSNode ts_node, const string& content, 
                                    const unordered_map<string, NodeConfig>& node_configs) {
    // Direct hash lookup - no virtual calls!
    auto config_it = node_configs.find(node.type.raw);
    const NodeConfig* config = (config_it != node_configs.end()) ? &config_it->second : nullptr;
    
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
        
        // NATIVE CONTEXT EXTRACTION: Use template specialization for zero-virtual-call performance
        if (config && config->native_strategy != NativeExtractionStrategy::NONE) {
            try {
                // Extract native context using compile-time template dispatch with error handling
                node.context.native = ExtractNativeContextTemplated<AdapterType>(ts_node, content, config->native_strategy);
                node.context.native_extraction_attempted = true;
            } catch (const std::exception& e) {
                // Log error but don't fail the entire parsing operation
                node.context.native_extraction_attempted = false;
                // Optionally log: printf("Native extraction error for node type '%s': %s\n", node.type.raw.c_str(), e.what());
            } catch (...) {
                // Catch any other errors
                node.context.native_extraction_attempted = false;
                // Optionally log: printf("Unknown native extraction error for node type '%s'\n", node.type.raw.c_str());
            }
        } else {
            // Explicitly mark that no extraction was attempted
            node.context.native_extraction_attempted = false;
        }
    } else {
        // Fallback: use PARSER_CONSTRUCT for unknown types
        node.context.normalized.semantic_type = SemanticTypes::PARSER_CONSTRUCT;
        node.context.normalized.universal_flags = 0;
        node.context.native_extraction_attempted = false;
    }
    
    // Set normalized type for display/compatibility
    node.type.normalized = SemanticTypes::GetSemanticTypeName(node.context.normalized.semantic_type);
    
    // Calculate arity binning
    node.context.normalized.arity_bin = ASTNode::BinArityFibonacci(ts_node_child_count(ts_node));
}

// Legacy template version for backward compatibility
template<typename AdapterType>
ASTResult UnifiedASTBackend::ParseToASTResultTemplated(const AdapterType* adapter,
                                                       const string& content, 
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
    
    // Call the new version
    return ParseToASTResultTemplated(adapter, content, language, file_path, config);
}

} // namespace duckdb