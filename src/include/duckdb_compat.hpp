#pragma once

#include "duckdb.hpp"
#include <type_traits>

// duckdb_compat.hpp — cross-version shim for DuckDB extensions.
//
// Pattern from @bendrucker's teaguesterling/duckdb_webbed#76 (May 2026):
// detect the new API via __has_include of headers that moved in the same DuckDB
// refactor ([duckdb/duckdb#22377](https://github.com/duckdb/duckdb/pull/22377) —
// "mandatory per-vector size tracking" landed alongside the vector-buffer
// header reshuffle), then dispatch via a single #ifdef block.
//
// Cross-version coverage:
//   - duckdb v1.4.x / v1.5.x: old API everywhere (built with -std=c++11)
//   - duckdb main / v1.6.x:   new API everywhere (built with -std=c++17)
//
// Important: this header is included on BOTH sides; nothing in it must require
// C++17 unconditionally. `std::optional` is only used inside the
// `DUCKDB_HAS_NEW_VECTOR_HEADERS` branches (which only compile on duckdb main,
// where C++17 is available). Forcing the whole extension to C++17 against
// duckdb v1.5.x's C++11 internals breaks linkage (static const data members
// in duckdb headers acquire implicit inline linkage in C++17 but not in C++11
// — multiple-definition errors at link time).
//
// See teaguesterling/duckdb_markdown's docs/DUCKDB_API_MIGRATION.md for the
// long-form rationale + upgrade checklist for other extensions.

#if __has_include("duckdb/common/vector/list_vector.hpp")
#define DUCKDB_HAS_NEW_VECTOR_HEADERS 1
#include "duckdb/common/vector/list_vector.hpp"
#include "duckdb/common/vector/struct_vector.hpp"
#include <optional> // C++17, only needed on the new-API path
#endif

// --- DuckDB v2.0 (the `main` line) -------------------------------------------
//
// The shims below cover the v2.0 API breaks. They are probed SEPARATELY from
// DUCKDB_HAS_NEW_VECTOR_HEADERS above and separately from each other: a version
// macro says *when* something changed, a probe says whether it changed *here*,
// which keeps working if a change is backported, reverted, or lands on a branch
// we did not expect. Tying several changes to one macro silently picks the
// wrong branch the moment they land in different releases.
//
// duckdb::Identifier replaced std::string as the name type in table-function
// and COPY bind signatures. Identifier compares case-insensitively, and
// construction from a RUNTIME string is explicit by design -- promoting a
// string to an identifier is meant to be a deliberate act at the call site --
// so boundary helpers are needed rather than an implicit conversion.
//
// This __has_include gates only whether a CompatNameStr(const Identifier &)
// overload can EXIST. It deliberately does NOT decide what CompatName is: the
// header was BACKPORTED to the stable branch without changing
// table_function_bind_t, so on v1.5-variegata's current tip identifier.hpp is
// present while binds still take vector<string>. Keying CompatName off the
// header would flip it to Identifier on the next submodule bump and break every
// bind signature at once. See CompatName below for what is used instead.
#if __has_include("duckdb/common/identifier.hpp")
#define DUCKDB_HAS_IDENTIFIER 1
#include "duckdb/common/identifier.hpp"
#endif

#include "duckdb/function/table_function.hpp"
#include <utility>

#include "duckdb/planner/expression/bound_function_expression.hpp"
#include "duckdb/parser/expression/comparison_expression.hpp"
#include "duckdb/parser/expression/conjunction_expression.hpp"
#include "duckdb/parser/expression/constant_expression.hpp"
#include "duckdb/parser/expression/function_expression.hpp"

// The scalar bind-function signature collapsed its three parameters into one
// input object on v2.0:
//   v1.5: (ClientContext &, ScalarFunction &, vector<unique_ptr<Expression>> &)
//   v2.0: (BindScalarFunctionInput &)
// Declare bind functions with DUCKDB_SCALAR_BIND_PARAMS and reach the pieces
// through DUCKDB_SCALAR_BIND_CONTEXT / DUCKDB_SCALAR_BIND_ARGS.
#ifdef DUCKDB_HAS_NEW_VECTOR_HEADERS
#define DUCKDB_SCALAR_BIND_PARAMS  duckdb::BindScalarFunctionInput &bind_input
#define DUCKDB_SCALAR_BIND_CONTEXT bind_input.GetClientContext()
#define DUCKDB_SCALAR_BIND_ARGS    bind_input.GetArguments()
#else
#define DUCKDB_SCALAR_BIND_PARAMS                                                                                      \
	duckdb::ClientContext &context, duckdb::ScalarFunction &bound_function,                                            \
	    duckdb::vector<duckdb::unique_ptr<duckdb::Expression>> &arguments
#define DUCKDB_SCALAR_BIND_CONTEXT context
#define DUCKDB_SCALAR_BIND_ARGS    arguments
#endif

namespace duckdb {

//===--------------------------------------------------------------------===//
// CompatName / CompatNameStr / CompatMakeName / CompatAssignNames
//===--------------------------------------------------------------------===//
//
// The bind-signature name type. Every table-function bind callback declares its
// names parameter as `vector<CompatName> &`; string LITERALS still work
// unchanged (`Identifier(const char *)` is implicit), so only the signatures
// move. Runtime strings crossing the boundary use CompatMakeName (in) and
// CompatNameStr (out).
//
// sitting_duck builds several of its column-name lists at runtime from the
// language/extraction configuration (UnifiedASTBackend::GetFlat...ColumnNames
// returns vector<string>), so CompatAssignNames exists for that case: it is the
// element-wise conversion that `names = helper()` used to be for free.

// Ask DuckDB what its own bind-name type is, rather than inferring it from a
// header's presence. TableFunctionBindInput::input_table_names has the same
// element type as the bind out-parameter on both lines -- vector<string> on the
// pin (table_function.hpp:110 / :288), vector<Identifier> on main (:123 / :319)
// -- so this cannot drift from the thing that actually changed. A probe on
// identifier.hpp can and does: that header is already present on
// v1.5-variegata's tip, where binds still take strings.
using CompatName = typename std::remove_reference<decltype(
    std::declval<TableFunctionBindInput &>().input_table_names)>::type::value_type;

inline string CompatNameStr(const string &name) {
	return name;
}
#ifdef DUCKDB_HAS_IDENTIFIER
inline string CompatNameStr(const Identifier &id) {
	return id.GetIdentifierName();
}
#endif

//! Promote a runtime string to whatever the bind-name type is. Named rather
//! than implicit because Identifier(const string &) is explicit by design.
inline CompatName CompatMakeName(string name) {
	return CompatName(std::move(name));
}

//! Replace `names` with the element-wise conversion of a runtime-built
//! vector<string>. Use where the pre-v2.0 code did `names = <helper>()`.
inline void CompatAssignNames(vector<CompatName> &names, const vector<string> &source) {
	names.clear();
	names.reserve(source.size());
	for (auto &name : source) {
		names.push_back(CompatMakeName(name));
	}
}

//===--------------------------------------------------------------------===//
// CompatWithAlias
//===--------------------------------------------------------------------===//
//
// v1.5: void SetAlias(string)                -- mutates in place
// v2.0: LogicalType WithAlias(string) const  -- returns a copy, never mutating
//       a type whose type-info may be shared. SetAlias is REMOVED, not
//       deprecated: on main the compiler says LogicalType "has no member named
//       SetAlias".
//
// Dispatched on a tag rather than with `if constexpr`, so this header also
// compiles at C++11 (see the linkage note at the top of the file -- forcing
// C++17 on the extension but not on libduckdb produces multiple-definition
// errors). Tag dispatch has the property that matters here: only the selected
// overload is instantiated, so the branch naming the absent member is never
// compiled. The member probe itself is valid C++11 in this form.

template <class T, class = void>
struct CompatHasWithAlias : std::false_type {};
template <class T>
struct CompatHasWithAlias<T, decltype(void(std::declval<const T &>().WithAlias(string())))> : std::true_type {};

template <class TYPE>
inline LogicalType CompatWithAliasImpl(TYPE type, string alias, std::true_type) {
	return type.WithAlias(std::move(alias));
}
template <class TYPE>
inline LogicalType CompatWithAliasImpl(TYPE type, string alias, std::false_type) {
	type.SetAlias(std::move(alias));
	return type;
}
// The entry point is deliberately NOT a template with a defaulted parameter.
// A default template argument is inert when deduction succeeds, so
// `CompatWithAlias(LogicalType::VARCHAR, "x")` would deduce TYPE =
// LogicalTypeId -- LogicalType::VARCHAR is a static constexpr LogicalTypeId,
// not a LogicalType -- and hard-error on the member lookup. Taking a concrete
// LogicalType by value converts at the call site instead. Only the Impl
// overloads are templates, which is all the tag dispatch needs.
inline LogicalType CompatWithAlias(LogicalType type, string alias) {
	return CompatWithAliasImpl(std::move(type), std::move(alias), CompatHasWithAlias<LogicalType>());
}

//===--------------------------------------------------------------------===//
// CompatFlatDataMutable
//===--------------------------------------------------------------------===//
//
// v1.5: FlatVector::GetData<T>(vec)         returns T*
// v2.0: FlatVector::GetData<T>(vec)         returns const T*
//       FlatVector::GetDataMutable<T>(vec)  returns T*
//
// Writing through the v2.0 read accessor is a compile error, which is the point
// of the split -- the WRITE path must ask for mutability explicitly. Note that
// ConstantVector::GetData<T> kept its non-const overload, so only FlatVector
// needs this.

template <class T, class = void>
struct CompatHasFlatGetDataMutable : std::false_type {};
template <class T>
struct CompatHasFlatGetDataMutable<T, decltype(void(T::template GetDataMutable<bool>(std::declval<Vector &>())))>
    : std::true_type {};

template <class VALUE, class FV>
inline VALUE *CompatFlatDataMutableImpl(Vector &vec, std::true_type) {
	return FV::template GetDataMutable<VALUE>(vec);
}
template <class VALUE, class FV>
inline VALUE *CompatFlatDataMutableImpl(Vector &vec, std::false_type) {
	return FV::template GetData<VALUE>(vec);
}
template <class VALUE, class FV = FlatVector>
inline VALUE *CompatFlatDataMutable(Vector &vec) {
	return CompatFlatDataMutableImpl<VALUE, FV>(vec, CompatHasFlatGetDataMutable<FV>());
}

//===--------------------------------------------------------------------===//
// CompatFlatValidityMutable
//===--------------------------------------------------------------------===//
//
// FlatVector::Validity got the same copy-on-write const split as GetData:
//   v2.0: const ValidityMask &Validity(const Vector &)   -- through Buffer()
//         ValidityMask &ValidityMutable(Vector &)        -- through BufferMutable()
//
// This one hides better than the GetData case. `auto &m = FlatVector::Validity(v);`
// still COMPILES on v2.0, silently deducing a CONST reference; the error only
// appears later at the mutation, as "passing 'const duckdb::ValidityMask' as
// 'this' argument discards qualifiers" -- naming neither Validity nor
// FlatVector. So grep for the MUTATION (SetInvalid / SetValid / SetAllInvalid /
// SetAllValid) and walk back to where the reference was bound.
//
// Use this ONLY on vectors being written. Reading an INPUT vector's validity
// through the mutable accessor would go through BufferMutable() and un-share a
// copy-on-write buffer for no reason.

template <class T, class = void>
struct CompatHasFlatValidityMutable : std::false_type {};
template <class T>
struct CompatHasFlatValidityMutable<T, decltype(void(T::ValidityMutable(std::declval<Vector &>())))> : std::true_type {
};

template <class FV>
inline ValidityMask &CompatFlatValidityMutableImpl(Vector &vec, std::true_type) {
	return FV::ValidityMutable(vec);
}
template <class FV>
inline ValidityMask &CompatFlatValidityMutableImpl(Vector &vec, std::false_type) {
	return FV::Validity(vec);
}
template <class FV = FlatVector>
inline ValidityMask &CompatFlatValidityMutable(Vector &vec) {
	return CompatFlatValidityMutableImpl<FV>(vec, CompatHasFlatValidityMutable<FV>());
}

//===--------------------------------------------------------------------===//
// CompatStructEntry / CompatStructGetField
//===--------------------------------------------------------------------===//
//
// v1.5: StructVector::GetEntries(vec) -> vector<unique_ptr<Vector>> &
// v2.0: StructVector::GetEntries(vec) -> vector<Vector> &
//
// So `*entries[i]` and `entries[i]->Foo()` stop compiling on v2.0. No probe is
// needed here: plain overload resolution distinguishes the two element types,
// and only the viable overload is ever selected. (A probe would also work, but
// this cannot get its polarity backwards.)

inline Vector &CompatStructEntry(Vector &entry) {
	return entry;
}
inline const Vector &CompatStructEntry(const Vector &entry) {
	return entry;
}
inline Vector &CompatStructEntry(const unique_ptr<Vector> &entry) {
	return *entry;
}

//! Child vector `field_idx` of a STRUCT vector, on either version.
inline Vector &CompatStructGetField(Vector &vec, idx_t field_idx) {
	return CompatStructEntry(StructVector::GetEntries(vec)[field_idx]);
}

//===--------------------------------------------------------------------===//
// CompatBoundBindInfo
//===--------------------------------------------------------------------===//
//
// BoundFunctionExpression::bind_info became private on v2.0; BindInfo() /
// BindInfoMutable() replace it. Execute callbacks read it through
// ExpressionState::expr, which is a const reference, so the const overload is
// the one that matters here.

#ifdef DUCKDB_HAS_NEW_VECTOR_HEADERS
inline const unique_ptr<FunctionData> &CompatBoundBindInfo(const BoundFunctionExpression &expr) {
	return expr.BindInfo();
}
inline unique_ptr<FunctionData> &CompatBoundBindInfo(BoundFunctionExpression &expr) {
	return expr.BindInfoMutable();
}
#else
inline const unique_ptr<FunctionData> &CompatBoundBindInfo(const BoundFunctionExpression &expr) {
	return expr.bind_info;
}
inline unique_ptr<FunctionData> &CompatBoundBindInfo(BoundFunctionExpression &expr) {
	return expr.bind_info;
}
#endif

//===--------------------------------------------------------------------===//
// Parsed-expression accessors
//===--------------------------------------------------------------------===//
//
// v2.0 made the parser expression hierarchy's data members private and added
// accessors; v1.5 has the public fields and (for most of these) no accessor, so
// a shim is needed in both directions rather than a straight rename.
//
//   ConstantExpression::value       -> GetValue()
//   FunctionExpression::function_name -> FunctionName()          (an Identifier)
//   FunctionExpression::children    -> GetArguments()            (vector<FunctionArgument>,
//                                                                 each wrapping an expression)
//   ComparisonExpression::left/right-> Left() / Right()
//   ConjunctionExpression::children -> GetChildren()
//
// The argument/child helpers return a vector of raw pointers rather than
// exposing either container shape, because the two versions no longer store the
// same thing: v2.0's FunctionExpression holds named FunctionArguments, not bare
// child expressions.

#ifdef DUCKDB_HAS_NEW_VECTOR_HEADERS

inline const Value &CompatConstantValue(const ConstantExpression &expr) {
	return expr.GetValue();
}
inline string CompatFunctionName(const FunctionExpression &expr) {
	return expr.FunctionName().GetIdentifierName();
}
inline vector<const ParsedExpression *> CompatFunctionArgExprs(const FunctionExpression &expr) {
	vector<const ParsedExpression *> result;
	for (auto &arg : expr.GetArguments()) {
		result.push_back(&arg.GetExpression());
	}
	return result;
}
inline const ParsedExpression *CompatComparisonLeft(const ComparisonExpression &expr) {
	return &expr.Left();
}
inline const ParsedExpression *CompatComparisonRight(const ComparisonExpression &expr) {
	return &expr.Right();
}
inline vector<const ParsedExpression *> CompatConjunctionChildren(const ConjunctionExpression &expr) {
	vector<const ParsedExpression *> result;
	for (auto &child : expr.GetChildren()) {
		result.push_back(child.get());
	}
	return result;
}

#else

inline const Value &CompatConstantValue(const ConstantExpression &expr) {
	return expr.value;
}
inline string CompatFunctionName(const FunctionExpression &expr) {
	return expr.function_name;
}
inline vector<const ParsedExpression *> CompatFunctionArgExprs(const FunctionExpression &expr) {
	vector<const ParsedExpression *> result;
	for (auto &child : expr.children) {
		result.push_back(child.get());
	}
	return result;
}
inline const ParsedExpression *CompatComparisonLeft(const ComparisonExpression &expr) {
	return expr.left.get();
}
inline const ParsedExpression *CompatComparisonRight(const ComparisonExpression &expr) {
	return expr.right.get();
}
inline vector<const ParsedExpression *> CompatConjunctionChildren(const ConjunctionExpression &expr) {
	vector<const ParsedExpression *> result;
	for (auto &child : expr.children) {
		result.push_back(child.get());
	}
	return result;
}

#endif

//===--------------------------------------------------------------------===//
// CompatSetOutputCardinality
//===--------------------------------------------------------------------===//

// On v2.0 this forwards to SetChildCardinality, which does strictly MORE than
// the deprecated SetCardinality (that one now just assigns the chunk's count via
// SetCardinalityUnsafe). Two things worth knowing before touching a call site:
//
// It is not destructive. DuckDB main's doc comment on SetCardinality warns that
// forwarding "would resize/overwrite their data", which reads alarming, but
// every leaf on the path is a size assignment behind a bounds check --
// DataChunk::SetChildCardinality -> FlatVector::SetSize ->
// VectorBuffer::SetVectorSize assigns v_size and reallocates nothing, and
// VectorStructBuffer::SetVectorSize only propagates the same count to children.
// No element bytes are touched, and there is no VectorListBuffer override, so
// LIST/MAP children are left alone too.
//
// It is also what makes this extension work at all under v2.0's per-vector size
// tracking. Every call site here writes children by INDEX first (raw
// CompatFlatDataMutable pointers, or Vector::SetValue) and sets the cardinality
// afterwards. Index writes never touch v_size, so the child vectors are still
// size 0 when the row loop ends; SetChildCardinality is what publishes them.
//
// The real hazard is LOUD, not silent: SetChildCardinality throws an
// InternalException if a column is neither flat nor constant and its size
// disagrees, or if the count exceeds a flat vector's capacity. Both are
// satisfied here -- output columns stay flat (ResetStructVectorState forces
// FLAT_VECTOR on struct children) and every producing loop is bounded by
// STANDARD_VECTOR_SIZE. A future call site that appends with Vector::Append,
// whose children are already sized, must not use this helper.
inline void CompatSetOutputCardinality(DataChunk &chunk, idx_t count) {
#ifdef DUCKDB_HAS_NEW_VECTOR_HEADERS
	chunk.SetChildCardinality(count);
#else
	chunk.SetCardinality(count);
#endif
}

//===--------------------------------------------------------------------===//
// SetValueCasted
//===--------------------------------------------------------------------===//
//
// Cross-version helper. On duckdb main, VectorStringBuffer::SetValue and
// StandardVectorBuffer::SetValue fall back to Value::DefaultCastAs(target_type)
// when val.type() != column.type(). DefaultCastAs uses a stack-local
// CastFunctionSet that does NOT see extension-registered casts
// (loader.RegisterCastFunction); the cast silently returns NULL and SetValue
// writes NULL. Pre-casting via Value::CastAs(ClientContext&, target_type) uses
// the catalog's cast set, which includes extension casts. Behaves identically
// on v1.4.x / v1.5.x where the old SetValue tolerated alias-only mismatches.
//
// yaml has a YAMLType alias on VARCHAR (see yaml_types.cpp), so this matters
// anywhere a Value(string) is written to a YAMLType-typed output column.

inline void SetValueCasted(ClientContext &context, Vector &vec, idx_t idx, const Value &val) {
	vec.SetValue(idx, val.CastAs(context, vec.GetType()));
}

//===--------------------------------------------------------------------===//
// CompatUnaryExecuteWithNulls / CompatBinaryExecuteWithNulls
//===--------------------------------------------------------------------===//
//
// Callsites use the OLD mask-based signature for the lambda:
//
//   CompatUnaryExecuteWithNulls<INPUT, RESULT>(
//       input, result, count,
//       [&](INPUT v, ValidityMask &mask, idx_t idx) -> RESULT {
//           if (!mask.RowIsValid(idx)) return RESULT{};       // NULL input
//           if (some_condition) {
//               mask.SetInvalid(idx);                          // explicit NULL output
//               return RESULT{};
//           }
//           return compute(v);
//       });
//
// The same lambda compiles against both v1.5.x and main:
//   - v1.5.x: forwarded directly to `UnaryExecutor::ExecuteWithNulls`.
//   - main: `ExecuteWithNulls` was removed in `987ea2c409`; we adapt by
//     constructing a fresh `ValidityMask` per row, calling the lambda with
//     it (idx=0), and translating its post-state to `std::optional<RESULT>`
//     for `UnaryExecutor::Execute`'s SFINAE-detected null-emitting overload.
//
// The "scratch mask" pattern keeps the C++17-only `std::optional` confined to
// the main-only branch, so the v1.5.x extension keeps building with `-std=c++11`
// alongside duckdb's C++11 internals.

#ifdef DUCKDB_HAS_NEW_VECTOR_HEADERS

template <class INPUT_TYPE, class RESULT_TYPE, class FUNC>
inline void CompatUnaryExecuteWithNulls(Vector &input, Vector &result, idx_t count, FUNC fun) {
	UnaryExecutor::Execute<INPUT_TYPE, RESULT_TYPE>(input, result, count, [fun](INPUT_TYPE in) -> std::optional<RESULT_TYPE> {
		ValidityMask scratch; // default-constructed → all rows valid
		RESULT_TYPE val = fun(in, scratch, idx_t(0));
		if (!scratch.RowIsValid(0)) {
			return std::nullopt;
		}
		return val;
	});
}

template <class LEFT_TYPE, class RIGHT_TYPE, class RESULT_TYPE, class FUNC>
inline void CompatBinaryExecuteWithNulls(Vector &left, Vector &right, Vector &result, idx_t count, FUNC fun) {
	BinaryExecutor::Execute<LEFT_TYPE, RIGHT_TYPE, RESULT_TYPE>(
	    left, right, result, count, [fun](LEFT_TYPE l, RIGHT_TYPE r) -> std::optional<RESULT_TYPE> {
		    ValidityMask scratch;
		    RESULT_TYPE val = fun(l, r, scratch, idx_t(0));
		    if (!scratch.RowIsValid(0)) {
			    return std::nullopt;
		    }
		    return val;
	    });
}

#else // v1.4.x / v1.5.x — pass the callsite lambda straight through

template <class INPUT_TYPE, class RESULT_TYPE, class FUNC>
inline void CompatUnaryExecuteWithNulls(Vector &input, Vector &result, idx_t count, FUNC fun) {
	UnaryExecutor::ExecuteWithNulls<INPUT_TYPE, RESULT_TYPE>(input, result, count, fun);
}

template <class LEFT_TYPE, class RIGHT_TYPE, class RESULT_TYPE, class FUNC>
inline void CompatBinaryExecuteWithNulls(Vector &left, Vector &right, Vector &result, idx_t count, FUNC fun) {
	BinaryExecutor::ExecuteWithNulls<LEFT_TYPE, RIGHT_TYPE, RESULT_TYPE>(left, right, result, count, fun);
}

#endif

} // namespace duckdb
