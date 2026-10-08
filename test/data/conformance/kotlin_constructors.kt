// Kotlin constructor-call fixture for the conformance kit.
//
// WHY THIS EXISTS: kotlin_types.def:85 declares
//     DEF_TYPE("constructor_invocation",
//              COMPUTATION_CALL | SemanticRefinements::Call::CONSTRUCTOR,
//              FIND_CALL_TARGET, FUNCTION_CALL, 0)
// so a `constructor_invocation` node must carry a callee name. The only
// occurrence of that node type in the existing committed corpus is in
// test/data/kotlin/simple.kt, which has an ERROR node at line 27 -- so the
// parse guard quarantines it and the kit could not see the failure from
// committed data at all.
//
// This file parses cleanly and contains the shape, so the finding is held by
// the corpus rather than by an ad-hoc reproduction.
//
// `: Base(n)` on line 7 is the constructor_invocation; `Base(1)` on line 10 is
// an ordinary call_expression, which DOES bind its name. Keeping both in one
// file makes the asymmetry the point rather than a guess.
open class Base(val n: Int)

class Derived(n: Int) : Base(n)

fun make(): Base {
    val b = Base(1)
    val d = Derived(2)
    return if (b.n > d.n) b else d
}
