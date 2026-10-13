// #231 fixture: one `as` token, two roles -- import alias and cast.
import kotlin.text.Regex as Rx
fun f(a: Any): String {
    val s = a as String
    return s
}
