fun helper(x: Int): Int {
    return x + 1
}

fun caller(obj: StringBuilder): Int {
    helper(1)
    obj.append("x")
    return helper(2)
}
