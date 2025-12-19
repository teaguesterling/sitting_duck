#pragma once

#include "duckdb.hpp"
#include "semantic_types.hpp"
#include <unordered_map>

namespace duckdb {

// Extraction strategy for node names/values
enum class ExtractionStrategy : uint8_t {
    NONE = 0,                   // No extraction needed
    NODE_TEXT = 1,              // Extract the node's own text content
    FIRST_CHILD = 2,            // Extract text from first child
    FIND_IDENTIFIER = 3,        // Find first child of type "identifier"
    FIND_PROPERTY = 4,          // Find first child of type "property_identifier"
    FIND_ASSIGNMENT_TARGET = 5, // Find identifier in parent assignment (universal pattern)
    FIND_QUALIFIED_IDENTIFIER = 6, // Find qualified/scoped identifiers and extract just the name part
    FIND_IN_DECLARATOR = 7,     // Find identifiers inside declarator nodes (universal pattern)
    FIND_CALL_TARGET = 8,       // Find method/function name from call (handles obj.method patterns)
    CUSTOM = 9                  // Language-specific custom logic
};

// Native context extraction strategies - pattern-based approach
enum class NativeExtractionStrategy : uint8_t {
    NONE = 0,                   // No native context extraction
    FUNCTION_WITH_PARAMS,       // Standard function with parameter list
    FUNCTION_WITH_DECORATORS,   // Function with annotations/decorators  
    ARROW_FUNCTION,             // Lambda/arrow function pattern
    ASYNC_FUNCTION,             // Async function pattern
    CLASS_WITH_INHERITANCE,     // Class with base classes
    CLASS_WITH_METHODS,         // Class with method definitions
    VARIABLE_WITH_TYPE,         // Typed variable assignment
    GENERIC_FUNCTION,           // Function with generic parameters
    METHOD_DEFINITION,          // Class/object method
    CONSTRUCTOR_DEFINITION,     // Constructor/initializer
    INTERFACE_DEFINITION,       // Interface/trait/protocol
    ENUM_DEFINITION,            // Enum/union type
    IMPORT_STATEMENT,           // Import/include/using
    EXPORT_STATEMENT,           // Export/public declarations
    FUNCTION_CALL,              // Function call/invocation with arguments
    CUSTOM = 255                // Language-specific custom logic
};

// Simple node configuration
struct NodeConfig {
    uint8_t semantic_type;              // 8-bit: bits 2-7 = type, bits 0-1 = refinement
    ExtractionStrategy name_strategy;   // How to extract names
    NativeExtractionStrategy native_strategy; // Native context extraction pattern (repurposed from value_strategy)
    uint8_t flags;                      // Basic flags (IS_KEYWORD, IS_PUBLIC, IS_UNSAFE)
    
    // Constructor
    NodeConfig(uint8_t sem_type = 0, 
               ExtractionStrategy name_strat = ExtractionStrategy::NONE,
               NativeExtractionStrategy native_strat = NativeExtractionStrategy::NONE,
               uint8_t node_flags = 0)
        : semantic_type(sem_type), name_strategy(name_strat), 
          native_strategy(native_strat), flags(node_flags) {}
};

// Universal flags for orthogonal node properties
namespace ASTNodeFlags {
    constexpr uint8_t IS_CONSTRUCT = 0x01;   // Semantic language construct (not just token/punctuation)
    constexpr uint8_t IS_EMBODIED = 0x02;    // Has body/implementation (definition vs declaration)
    // 0x04 - 0x80: Reserved for future use

    // Backward compatibility aliases (deprecated - will be removed)
    constexpr uint8_t IS_KEYWORD = IS_CONSTRUCT;
    constexpr uint8_t IS_KEYWORD_IF_LEAF = IS_CONSTRUCT;  // Treat same as IS_CONSTRUCT
}

// Semantic refinement constants for fine-grained classification
namespace SemanticRefinements {
    // DEFINITION_FUNCTION refinements (bits 0-1)
    namespace Function {
        constexpr uint8_t REGULAR = 0x00;     // 00 - Named functions, methods, procedures
        constexpr uint8_t LAMBDA = 0x01;      // 01 - Anonymous functions, closures, arrows
        constexpr uint8_t CONSTRUCTOR = 0x02; // 10 - Constructors, initializers, destructors
        constexpr uint8_t ASYNC = 0x03;       // 11 - Async, generator, coroutine functions
    }
    
    // LITERAL_NUMBER refinements
    namespace Number {
        constexpr uint8_t INTEGER = 0x00;      // 00 - All integer formats (decimal, hex, binary)
        constexpr uint8_t FLOAT = 0x01;        // 01 - Floating point numbers
        constexpr uint8_t SCIENTIFIC = 0x02;   // 10 - Scientific notation (1.23e-4, 2E+5)
        constexpr uint8_t COMPLEX = 0x03;      // 11 - Complex numbers, rationals
    }
    
    // LITERAL_STRUCTURED refinements
    namespace Structured {
        constexpr uint8_t GENERIC = 0;      // 00 - Unspecified structured literals
        constexpr uint8_t SEQUENCE = 1;     // 01 - Arrays, lists, tuples, vectors
        constexpr uint8_t MAPPING = 2;      // 10 - Objects, dictionaries, hashmaps
        constexpr uint8_t SET = 3;          // 11 - Sets, collections, unique containers
    }
    
    // OPERATOR_ARITHMETIC refinements
    namespace Arithmetic {
        constexpr uint8_t BINARY = 0x00;       // 00 - +, -, *, /, %, standard binary ops
        constexpr uint8_t UNARY = 0x01;        // 01 - ++, --, unary +/-, sizeof
        constexpr uint8_t BITWISE = 0x02;      // 10 - &, |, ^, <<, >>, bitwise operations
        constexpr uint8_t RANGE = 0x03;        // 11 - .., ..=, range and interval operators
    }
    
    // FLOW_CONDITIONAL refinements  
    namespace Conditional {
        constexpr uint8_t BINARY = 0;       // 00 - if/else, unless, binary decisions
        constexpr uint8_t MULTIWAY = 1;     // 01 - switch/case, match/when, pattern matching
        constexpr uint8_t GUARD = 2;        // 10 - guard statements, assertions, preconditions
        constexpr uint8_t TERNARY = 3;      // 11 - ?: operators, conditional expressions
    }
    
    // FLOW_LOOP refinements
    namespace Loop {
        constexpr uint8_t COUNTER = 0;      // 00 - for(int i=0; i<n; i++), counting loops
        constexpr uint8_t ITERATOR = 1;     // 01 - for-in, for-of, foreach, iterator-based
        constexpr uint8_t CONDITIONAL = 2;  // 10 - while, until, condition-based loops
        constexpr uint8_t INFINITE = 3;     // 11 - loop, repeat, infinite loop constructs
    }
    
    // ORGANIZATION refinements
    namespace Organization {
        constexpr uint8_t SEQUENTIAL = 0;   // 00 - Code blocks, statement sequences
        constexpr uint8_t COLLECTION = 1;   // 01 - Parameter lists, argument lists
        constexpr uint8_t MAPPING = 2;      // 10 - Named containers, objects
        constexpr uint8_t HIERARCHICAL = 3; // 11 - Modules, namespaces, packages
    }
    
    // DEFINITION_VARIABLE refinements
    namespace Variable {
        constexpr uint8_t MUTABLE = 0x00;      // 00 - var, let, mutable variables
        constexpr uint8_t IMMUTABLE = 0x01;    // 01 - const, final, readonly
        constexpr uint8_t PARAMETER = 0x02;    // 10 - Function/method parameters
        constexpr uint8_t FIELD = 0x03;        // 11 - Class/struct fields, properties
    }
    
    // COMPUTATION_CALL refinements
    namespace Call {
        constexpr uint8_t FUNCTION = 0x00;     // 00 - Regular function calls
        constexpr uint8_t METHOD = 0x01;       // 01 - Object method calls  
        constexpr uint8_t CONSTRUCTOR = 0x02;  // 10 - new ClassName(), constructors
        constexpr uint8_t MACRO = 0x03;        // 11 - Preprocessor macros, compile-time
    }
    
    // EXTERNAL_IMPORT refinements
    namespace Import {
        constexpr uint8_t MODULE = 0x00;       // 00 - import module, #include
        constexpr uint8_t SELECTIVE = 0x01;    // 01 - from module import specific
        constexpr uint8_t WILDCARD = 0x02;     // 10 - import *, using namespace
        constexpr uint8_t RELATIVE = 0x03;     // 11 - from . import, relative imports
    }
    
    // LITERAL_STRING refinements
    namespace String {
        constexpr uint8_t LITERAL = 0x00;      // 00 - Basic quoted strings
        constexpr uint8_t TEMPLATE = 0x01;     // 01 - Template strings, f-strings, interpolation
        constexpr uint8_t REGEX = 0x02;        // 10 - Regular expressions
        constexpr uint8_t RAW = 0x03;          // 11 - Raw strings, here-docs, verbatim
    }
    
    // OPERATOR_COMPARISON refinements
    namespace Comparison {
        constexpr uint8_t EQUALITY = 0x00;     // 00 - ==, ===, !=, !==
        constexpr uint8_t RELATIONAL = 0x01;   // 01 - <, >, <=, >=
        constexpr uint8_t MEMBERSHIP = 0x02;   // 10 - in, instanceof, typeof
        constexpr uint8_t PATTERN = 0x03;      // 11 - =~, match, regex comparisons
    }
    
    // OPERATOR_ASSIGNMENT refinements  
    namespace Assignment {
        constexpr uint8_t SIMPLE = 0x00;       // 00 - =, :=
        constexpr uint8_t COMPOUND = 0x01;     // 01 - +=, -=, *=, /=
        constexpr uint8_t DESTRUCTURE = 0x02;  // 10 - [a,b] = arr, {x,y} = obj
        constexpr uint8_t AUGMENTED = 0x03;    // 11 - ||=, &&=, ??=
    }
    
    // DEFINITION_CLASS refinements
    namespace Class {
        constexpr uint8_t REGULAR = 0x00;      // 00 - Basic classes
        constexpr uint8_t ABSTRACT = 0x01;     // 01 - Abstract classes, interfaces
        constexpr uint8_t GENERIC = 0x02;      // 10 - Template/generic classes
        constexpr uint8_t ENUM = 0x03;         // 11 - Enums, union types
    }
    
    // FLOW_JUMP refinements
    namespace Jump {
        constexpr uint8_t RETURN = 0x00;       // 00 - return, yield return
        constexpr uint8_t BREAK = 0x01;        // 01 - break, exit loops
        constexpr uint8_t CONTINUE = 0x02;     // 10 - continue, next, skip
        constexpr uint8_t GOTO = 0x03;         // 11 - goto, unconditional jumps
    }
    
    // NAME_IDENTIFIER refinements
    namespace Identifier {
        constexpr uint8_t VARIABLE = 0x00;     // 00 - Variable references
        constexpr uint8_t FUNCTION = 0x01;     // 01 - Function name references
        constexpr uint8_t TYPE = 0x02;         // 10 - Class/type name references
        constexpr uint8_t LABEL = 0x03;        // 11 - Labels, tags
    }
    
    
    // Cross-language query/data patterns (existing, keeping for compatibility)
    namespace Query {
        constexpr uint8_t SIMPLE = 0x00;      // 00 - Basic queries/comprehensions/selects
        constexpr uint8_t NESTED = 0x01;      // 01 - Nested queries/comprehensions/subqueries
        constexpr uint8_t FILTERED = 0x02;    // 10 - With WHERE/filter clauses
        constexpr uint8_t GROUPED = 0x03;     // 11 - With GROUP BY/grouping operations
    }
    
    namespace Aggregation {
        constexpr uint8_t SIMPLE = 0x00;      // 00 - Basic reduction (sum, count, reduce)
        constexpr uint8_t CONDITIONAL = 0x01; // 01 - Conditional aggregation (filter-then-reduce)
        constexpr uint8_t WINDOWED = 0x02;    // 10 - Rolling/windowed operations (sliding window)
        constexpr uint8_t GROUPED = 0x03;     // 11 - Group-based aggregation (group by, partition)
    }
    
    namespace Iteration {
        constexpr uint8_t MAP = 0x00;         // 00 - Transform operations (map, select, transform)
        constexpr uint8_t FILTER = 0x01;      // 01 - Filter operations (where, filter, find)
        constexpr uint8_t REDUCE = 0x02;      // 10 - Reduction operations (fold, reduce, aggregate)
        constexpr uint8_t FLAT = 0x03;        // 11 - Flattening operations (flatMap, flatten, SelectMany)
    }
    
    namespace Join {
        constexpr uint8_t INNER = 0x00;       // 00 - Inner joins/intersections/zip
        constexpr uint8_t LEFT = 0x01;        // 01 - Left joins/left-biased operations
        constexpr uint8_t RIGHT = 0x02;       // 10 - Right joins/right-biased operations  
        constexpr uint8_t OUTER = 0x03;       // 11 - Full outer joins/unions/concatenation
    }
    
    // Generic refinement for types that don't need specific refinements
    namespace Generic {
        constexpr uint8_t UNSPECIFIED = 0x00;  // 00 - Default for types without refinements
        constexpr uint8_t RESERVED1 = 0x01;    // 01 - Reserved for future use
        constexpr uint8_t RESERVED2 = 0x02;    // 10 - Reserved for future use
        constexpr uint8_t RESERVED3 = 0x03;    // 11 - Reserved for future use
    }
}

// Normalized type constants for cross-language consistency
namespace NormalizedTypes {
    // Declarations/Definitions
    constexpr const char* FUNCTION_DECLARATION = "function_declaration";
    constexpr const char* CLASS_DECLARATION = "class_declaration";
    constexpr const char* VARIABLE_DECLARATION = "variable_declaration";
    constexpr const char* METHOD_DECLARATION = "method_declaration";
    constexpr const char* PARAMETER_DECLARATION = "parameter_declaration";
    
    // Expressions/Computations
    constexpr const char* FUNCTION_CALL = "function_call";
    constexpr const char* VARIABLE_REFERENCE = "variable_reference";
    constexpr const char* BINARY_EXPRESSION = "binary_expression";
    constexpr const char* UNARY_EXPRESSION = "unary_expression";
    constexpr const char* ASSIGNMENT_EXPRESSION = "assignment_expression";
    
    // Literals
    constexpr const char* STRING_LITERAL = "string_literal";
    constexpr const char* NUMBER_LITERAL = "number_literal";
    constexpr const char* BOOLEAN_LITERAL = "boolean_literal";
    constexpr const char* NULL_LITERAL = "null_literal";
    
    // Control Flow
    constexpr const char* IF_STATEMENT = "if_statement";
    constexpr const char* FOR_STATEMENT = "for_statement";
    constexpr const char* WHILE_STATEMENT = "while_statement";
    constexpr const char* RETURN_STATEMENT = "return_statement";
    constexpr const char* BREAK_STATEMENT = "break_statement";
    constexpr const char* CONTINUE_STATEMENT = "continue_statement";
    
    // Structure/Organization
    constexpr const char* BLOCK = "block";
    constexpr const char* MODULE = "module";
    constexpr const char* IMPORT_STATEMENT = "import_statement";
    constexpr const char* EXPORT_STATEMENT = "export_statement";
    
    // Other
    constexpr const char* COMMENT = "comment";
    constexpr const char* IDENTIFIER = "identifier";
    constexpr const char* OPERATOR = "operator";
    constexpr const char* PUNCTUATION = "punctuation";
}

} // namespace duckdb