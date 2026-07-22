func helper(_ x: Int) -> Int {
    return x + 1
}

func caller(text: String) -> Int {
    let upper = text.uppercased()
    return helper(upper.count)
}
