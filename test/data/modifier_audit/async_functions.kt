import kotlinx.coroutines.*

suspend fun fetchData(url: String): String {
    return withContext(Dispatchers.IO) { "data" }
}

fun syncHelper(): Int {
    return 42
}

private suspend fun privateAsync(): Unit {
    delay(1000)
}

inline fun inlineHelper(block: () -> Unit) {
    block()
}
