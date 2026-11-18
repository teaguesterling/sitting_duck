#include "duckdb.hpp"
#include "duckdb_compat.hpp"
#include "duckdb/function/cast/cast_function_set.hpp"
#include "duckdb/function/cast/default_casts.hpp"
#include "include/semantic_types.hpp"

namespace duckdb {

// Type name constant
static constexpr const char *SEMANTIC_TYPE_NAME = "SEMANTIC_TYPE";

// Create the SEMANTIC_TYPE logical type (UTINYINT with alias)
LogicalType SemanticTypeLogicalType() {
	// DuckDB v2.0 removed LogicalType::SetAlias (it mutated a type whose
	// type-info may be shared) in favour of WithAlias, which returns a copy.
	// CompatWithAlias picks whichever exists; the resulting type is identical.
	return CompatWithAlias(LogicalType(LogicalTypeId::UTINYINT), SEMANTIC_TYPE_NAME);
}

// Check if a type is SEMANTIC_TYPE
bool IsSemanticType(const LogicalType &type) {
	return type.id() == LogicalTypeId::UTINYINT && type.HasAlias() && type.GetAlias() == SEMANTIC_TYPE_NAME;
}

//------------------------------------------------------------------------------
// Cast: SEMANTIC_TYPE -> VARCHAR
//------------------------------------------------------------------------------

static bool SemanticTypeToVarcharCast(Vector &source, Vector &result, idx_t count, CastParameters &parameters) {
	UnaryExecutor::Execute<uint8_t, string_t>(source, result, count, [&](uint8_t semantic_type) {
		// Handle refinement bits for backward compatibility
		uint8_t refinement_bits = semantic_type & 0x03;
		uint8_t base_semantic_type = semantic_type & 0xFC;

		if (refinement_bits != 0) {
			string base_type_name = SemanticTypes::GetSemanticTypeName(base_semantic_type);
			if (base_type_name != "UNKNOWN_SEMANTIC_TYPE") {
				return StringVector::AddString(result, base_type_name);
			}
		}

		string type_name = SemanticTypes::GetSemanticTypeName(semantic_type);
		return StringVector::AddString(result, type_name);
	});
	return true;
}

//------------------------------------------------------------------------------
// Cast: VARCHAR -> SEMANTIC_TYPE
//------------------------------------------------------------------------------

static bool VarcharToSemanticTypeCast(Vector &source, Vector &result, idx_t count, CastParameters &parameters) {
	// Note: we use the `mask` lambda parameter (not a captured
	// FlatVector::Validity(result)) to emit NULL outputs. The CompatExecuteWithNulls
	// wrappers on duckdb main pass a scratch per-row mask to the lambda and
	// translate `mask.SetInvalid` to `std::nullopt` for the new Execute API;
	// writing to result's validity directly would be bypassed there.
	CompatUnaryExecuteWithNulls<string_t, uint8_t>(source, result, count,
	                                               [&](string_t name_str, ValidityMask &mask, idx_t idx) -> uint8_t {
		                                               string name = name_str.GetString();
		                                               uint8_t code = SemanticTypes::GetSemanticTypeCode(name);
		                                               if (code == 255) {
			                                               mask.SetInvalid(idx); // Unknown type name → SQL NULL
			                                               return 0;
		                                               }
		                                               return code;
	                                               });
	return true;
}

//------------------------------------------------------------------------------
// Registration
//------------------------------------------------------------------------------

void RegisterSemanticTypeLogicalType(ExtensionLoader &loader) {
	auto &db = loader.GetDatabaseInstance();

	// Get the SEMANTIC_TYPE logical type
	auto semantic_type = SemanticTypeLogicalType();

	// Register the TYPE ITSELF, not only its casts (#244).
	//
	// Without this the alias rides along on values -- typeof() and DESCRIBE both
	// report SEMANTIC_TYPE -- but the name cannot be resolved in the catalog, so
	// anything that looks it up by name fails. The full-sort path does:
	//
	//     SELECT type FROM read_ast('f.js') ORDER BY semantic_type;
	//     -> Catalog Error: Type with name SEMANTIC_TYPE does not exist!
	//
	// which is an ordinary query failing with a message that points at nothing
	// the user wrote. It looked intermittent because `LIMIT` plans top-N instead
	// of a sort and so dodges it, as do GROUP BY, max() and an explicit
	// ::VARCHAR cast -- none of which need the name.
	//
	// Registered BEFORE the casts, matching duckdb/extension/json's
	// json_extension.cpp (type, then cast functions). ExtensionLoader::RegisterType
	// creates the entry as temporary + internal in the system catalog, so it is
	// session-scoped and disappears with the extension, exactly as JSON's does.
	loader.RegisterType(SEMANTIC_TYPE_NAME, semantic_type);

	// SEMANTIC_TYPE -> VARCHAR: Display the human-readable name
	// Cost of 1 means it's nearly free (like JSON -> VARCHAR)
	loader.RegisterCastFunction(semantic_type, LogicalType::VARCHAR, SemanticTypeToVarcharCast, 1);

	// VARCHAR -> SEMANTIC_TYPE: Parse the type name
	// Higher cost since it requires string lookup
	auto varchar_to_semantic_cost =
	    CastFunctionSet::ImplicitCastCost(db, LogicalType::SQLNULL, LogicalTypeId::STRUCT) + 1;
	loader.RegisterCastFunction(LogicalType::VARCHAR, semantic_type, VarcharToSemanticTypeCast,
	                            varchar_to_semantic_cost);

	// UTINYINT -> SEMANTIC_TYPE: Just reinterpret (same underlying type)
	// Cost of 1 makes this very cheap for implicit conversion
	loader.RegisterCastFunction(LogicalType::UTINYINT, semantic_type, DefaultCasts::ReinterpretCast, 1);

	// SEMANTIC_TYPE -> UTINYINT: Just reinterpret (same underlying type)
	// Cost of 1 makes this very cheap - needed for using with functions that take UTINYINT
	loader.RegisterCastFunction(semantic_type, LogicalType::UTINYINT, DefaultCasts::ReinterpretCast, 1);
}

} // namespace duckdb
