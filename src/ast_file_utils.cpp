#include "ast_file_utils.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/file_system.hpp"
#include <algorithm>
#include <unordered_map>

namespace duckdb {

// Static mapping of file extensions to languages
static const std::unordered_map<string, string> EXTENSION_TO_LANGUAGE = {
    {"cpp", "cpp"}, {"cc", "cpp"}, {"cxx", "cpp"}, {"c++", "cpp"},
    {"hpp", "cpp"}, {"hh", "cpp"}, {"hxx", "cpp"}, {"h++", "cpp"},
    {"c", "c"}, {"h", "c"},
    {"py", "python"}, {"pyi", "python"}, {"pyw", "python"},
    {"js", "javascript"}, {"jsx", "javascript"}, {"mjs", "javascript"},
    {"ts", "typescript"}, {"tsx", "typescript"},
    {"go", "go"},
    {"rb", "ruby"}, {"ruby", "ruby"},
    {"sql", "sql"},
    {"rs", "rust"}, {"rlib", "rust"},
    {"md", "markdown"}, {"markdown", "markdown"},
    {"java", "java"},
    // PHP enabled - scanner dependency resolved
    {"php", "php"}, {"php3", "php"}, {"php4", "php"}, {"php5", "php"}, {"phtml", "php"},
    {"html", "html"}, {"htm", "html"},
    {"css", "css"},
    {"json", "json"},
    // YAML grammar has complex self-modifying structure incompatible with tree-sitter CLI
    // {"yaml", "yaml"}, {"yml", "yaml"},
    {"sh", "bash"}, {"bash", "bash"}, {"zsh", "bash"},
    {"swift", "swift"},
    {"r", "r"}, {"R", "r"},
    {"kt", "kotlin"}, {"kts", "kotlin"},
    {"cs", "csharp"},
    {"lua", "lua"},
    {"hcl", "hcl"}, {"tf", "hcl"}, {"tfvars", "hcl"},
    {"graphql", "graphql"}, {"gql", "graphql"},
    {"toml", "toml"}
};

// Static mapping of languages to their supported extensions
static const std::unordered_map<string, vector<string>> LANGUAGE_TO_EXTENSIONS = {
    {"cpp", {"cpp", "cc", "cxx", "c++", "hpp", "hh", "hxx", "h++"}},
    {"c", {"c", "h"}},
    {"python", {"py", "pyi", "pyw"}},
    {"javascript", {"js", "jsx", "mjs"}},
    {"typescript", {"ts", "tsx"}},
    {"go", {"go"}},
    {"ruby", {"rb", "ruby"}},
    {"sql", {"sql"}},
    {"rust", {"rs", "rlib"}},
    {"markdown", {"md", "markdown"}},
    {"java", {"java"}},
    {"php", {"php", "php3", "php4", "php5", "phtml"}}, // Enabled
    {"html", {"html", "htm"}},
    {"css", {"css"}},
    {"json", {"json"}},
    // YAML grammar has complex self-modifying structure incompatible with tree-sitter CLI
    // {"yaml", {"yaml", "yml"}},
    {"bash", {"sh", "bash", "zsh"}},
    {"swift", {"swift"}},
    {"r", {"r", "R"}},
    {"kotlin", {"kt", "kts"}},
    {"csharp", {"cs"}},
    {"lua", {"lua"}},
    {"hcl", {"hcl", "tf", "tfvars"}},
    {"graphql", {"graphql", "gql"}},
    {"toml", {"toml"}}
};

vector<string> ASTFileUtils::GetFiles(ClientContext &context, const Value &path_value, 
                                     bool ignore_errors,
                                     const vector<string> &supported_extensions) {
    vector<string> result;

    // Helper lambda to handle individual file paths
    auto processPath = [&](const string& file_path) {
        auto files = ProcessSinglePath(context, file_path, supported_extensions, ignore_errors);
        result.insert(result.end(), files.begin(), files.end());
    };

    // Handle list of files
    if (path_value.type().id() == LogicalTypeId::LIST) {
        auto &file_list = ListValue::GetChildren(path_value);
        for (auto &file_value : file_list) {
            if (file_value.type().id() == LogicalTypeId::VARCHAR) {
                processPath(file_value.ToString());
            } else {
                throw BinderException("File list must contain string values");
            }
        }
    // Handle string path (file, glob pattern, or directory)
    } else if (path_value.type().id() == LogicalTypeId::VARCHAR) {
        processPath(path_value.ToString());
    // Handle invalid types
    } else {
        throw BinderException("File path must be a string or list of strings");
    }

    return result;
}

vector<string> ASTFileUtils::ProcessSinglePath(ClientContext &context, const string &path,
                                              const vector<string> &supported_extensions,
                                              bool ignore_errors) {
    auto &fs = FileSystem::GetFileSystem(context);
    vector<string> result;

    // Single file
    if (fs.FileExists(path)) {
        // Check if file extension is supported
        if (supported_extensions.empty() || IsFileExtensionSupported(path, supported_extensions)) {
            result.push_back(path);
        }
    // Single directory
    } else if (fs.DirectoryExists(path)) {
        // Get all supported files in directory
        if (supported_extensions.empty()) {
            // If no extensions specified, get all files (this might be too broad)
            auto all_files = GetGlobFiles(context, fs.JoinPath(path, "*"), supported_extensions);
            result.insert(result.end(), all_files.begin(), all_files.end());
        } else {
            // Get files for each supported extension
            for (const auto &ext : supported_extensions) {
                auto pattern = fs.JoinPath(path, "*." + ext);
                auto ext_files = GetGlobFiles(context, pattern, supported_extensions);
                result.insert(result.end(), ext_files.begin(), ext_files.end());
            }
        }
    // Glob pattern
    } else if (fs.HasGlob(path)) {
        auto glob_files = GetGlobFiles(context, path, supported_extensions);
        result.insert(result.end(), glob_files.begin(), glob_files.end());
    // Don't fail if ignore_errors is true
    } else if (!ignore_errors) {
        throw IOException("File or directory does not exist: " + path);
    }

    return result;
}

vector<string> ASTFileUtils::GetGlobFiles(ClientContext &context, const string &pattern,
                                         const vector<string> &supported_extensions) {
    auto &fs = FileSystem::GetFileSystem(context);
    vector<string> result;

    // Given a glob path, add any file results (ignoring directories)
    auto globFileResults = [&result, &fs, &supported_extensions](const string& glob_path) {
        for (auto &file : fs.Glob(glob_path)) {
            if (!fs.DirectoryExists(file.path)) {
                // Check if file extension is supported
                if (supported_extensions.empty() || IsFileExtensionSupported(file.path, supported_extensions)) {
                    result.push_back(file.path);
                }
            }
        }
    };

    // Check if it's already a glob pattern
    if (fs.HasGlob(pattern)) {
        globFileResults(pattern);
    } else if (fs.DirectoryExists(pattern)) {
        // If it's a directory, look for supported files inside
        if (supported_extensions.empty()) {
            globFileResults(fs.JoinPath(pattern, "*"));
        } else {
            for (const auto &ext : supported_extensions) {
                globFileResults(fs.JoinPath(pattern, "*." + ext));
            }
        }
    } else {
        // If it's not a directory or glob, pass it through as is
        if (supported_extensions.empty() || IsFileExtensionSupported(pattern, supported_extensions)) {
            result.push_back(pattern);
        }
    }

    return result;
}

string ASTFileUtils::DetectLanguageFromPath(const string &file_path) {
    // Extract file extension
    auto last_dot = file_path.find_last_of('.');
    if (last_dot == string::npos) {
        return "auto"; // No extension found
    }
    
    string extension = file_path.substr(last_dot + 1);
    
    // Convert to lowercase for case-insensitive matching
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    
    // Look up the language
    auto it = EXTENSION_TO_LANGUAGE.find(extension);
    if (it != EXTENSION_TO_LANGUAGE.end()) {
        return it->second;
    }
    
    return "auto"; // Extension not recognized
}

bool ASTFileUtils::IsFileTypeSupported(const string &file_path, const string &language) {
    auto detected_language = DetectLanguageFromPath(file_path);
    return detected_language == language;
}

vector<string> ASTFileUtils::GetSupportedExtensions(const string &language) {
    auto it = LANGUAGE_TO_EXTENSIONS.find(language);
    if (it != LANGUAGE_TO_EXTENSIONS.end()) {
        return it->second;
    }
    return {}; // Language not recognized
}

// Helper function to check if a file extension is in the supported list
bool ASTFileUtils::IsFileExtensionSupported(const string &file_path, const vector<string> &supported_extensions) {
    // Extract file extension
    auto last_dot = file_path.find_last_of('.');
    if (last_dot == string::npos) {
        return false; // No extension found
    }
    
    string extension = file_path.substr(last_dot + 1);
    
    // Convert to lowercase for case-insensitive matching
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    
    // Check if extension is in the supported list
    return std::find(supported_extensions.begin(), supported_extensions.end(), extension) != supported_extensions.end();
}

// New overload for multiple patterns (DuckDB-consistent glob array support)
vector<string> ASTFileUtils::GetFiles(ClientContext &context, const vector<string> &patterns,
                                     bool ignore_errors,
                                     const vector<string> &supported_extensions) {
    vector<string> all_files;
    
    // Process each pattern and collect files
    for (const auto &pattern : patterns) {
        try {
            auto pattern_files = ProcessSinglePath(context, pattern, supported_extensions, ignore_errors);
            all_files.insert(all_files.end(), pattern_files.begin(), pattern_files.end());
        } catch (const Exception &e) {
            if (!ignore_errors) {
                throw IOException("Failed to process pattern '" + pattern + "': " + string(e.what()));
            }
            // With ignore_errors=true, continue processing other patterns
        }
    }
    
    // Sort for consistent ordering (following DuckDB conventions)
    std::sort(all_files.begin(), all_files.end());
    
    // Remove duplicates (files may match multiple patterns)
    all_files.erase(std::unique(all_files.begin(), all_files.end()), all_files.end());
    
    return all_files;
}

} // namespace duckdb