// #231 fixture: `operator` declares an operator; `as` is a cast.
infix operator |>: AdditionPrecedence
func f(a: Any) -> String {
  let s = a as String
  return s
}
