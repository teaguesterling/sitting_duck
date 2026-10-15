// #220 fixture: kotlin parameter shapes. The default value and the `vararg`
// marker both sit OUTSIDE the `parameter` node, so they exercise the
// sibling-tracking the extractor has to do.
fun noargs(): Int = 1

fun greet(who: String): String = "hi $who"

fun add(a: Int, b: Int = 2, vararg rest: String): Int = a + b

fun nullableParam(maybe: String?): Int = 0
