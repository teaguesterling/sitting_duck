#pragma once

#include "duckdb.hpp"
#include "duckdb/common/file_system.hpp"
#include "duckdb/main/client_context.hpp"

namespace duckdb {

/**
 * @brief Utility class for handling file operations in AST functions
 * 
 * Supports single files, file lists, glob patterns, and directories.
 * Based on the patterns used in the duckdb_yaml extension.
 */
class ASTFileUtils {
public:
    /**
     * @brief Get files from a Value (which can be a string or list of strings)
     *
     * @param context Client context for file operations
     * @param path_value The input value containing file path(s)
     * @param ignore_errors Whether to ignore missing files
     * @param supported_extensions List of supported file extensions (e.g., {"cpp", "hpp", "py"})
     * @return vector<string> List of resolved file paths
     */
    static vector<string> GetFiles(ClientContext &context, const Value &path_value, 
                                  bool ignore_errors = false,
                                  const vector<string> &supported_extensions = {});

    /**
     * @brief Get files from a glob pattern
     *
     * @param context Client context for file operations
     * @param pattern Glob pattern to match files
     * @param supported_extensions List of supported file extensions
     * @return vector<string> List of matching file paths
     */
    static vector<string> GetGlobFiles(ClientContext &context, const string &pattern,
                                      const vector<string> &supported_extensions = {});

    /**
     * @brief Auto-detect language from file extension
     *
     * @param file_path Path to the file
     * @return string Language identifier or "auto" if not detected
     */
    static string DetectLanguageFromPath(const string &file_path);

    /**
     * @brief Check if a file extension is supported for a given language
     *
     * @param file_path Path to the file
     * @param language Language to check against
     * @return bool True if the file extension matches the language
     */
    static bool IsFileTypeSupported(const string &file_path, const string &language);

    /**
     * @brief Get all supported extensions for a language
     *
     * @param language Language identifier
     * @return vector<string> List of supported extensions (without dots)
     */
    static vector<string> GetSupportedExtensions(const string &language);

private:
    /**
     * @brief Process a single path (file, directory, or glob pattern)
     *
     * @param context Client context for file operations
     * @param path Single path to process
     * @param supported_extensions List of supported file extensions
     * @param ignore_errors Whether to ignore missing files
     * @return vector<string> List of matching files
     */
    static vector<string> ProcessSinglePath(ClientContext &context, const string &path,
                                           const vector<string> &supported_extensions,
                                           bool ignore_errors);

    /**
     * @brief Check if a file extension is in the supported extensions list
     *
     * @param file_path Path to the file
     * @param supported_extensions List of supported extensions
     * @return bool True if the extension is supported
     */
    static bool IsFileExtensionSupported(const string &file_path, const vector<string> &supported_extensions);
};

} // namespace duckdb