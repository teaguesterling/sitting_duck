// Simple Kotlin example demonstrating modern language features
data class Person(val name: String, val age: Int)

fun main() {
    val greeting = "Hello, Kotlin!"
    println(greeting)
    
    // Lambda expressions with semantic refinements
    val numbers = listOf(1, 2, 3, 4, 5)
    val doubled = numbers.map { it * 2 }
    
    // Higher-order functions
    val transform: (Int) -> Int = { x -> x * x }
    val squared = numbers.map(transform)
    
    // Immutable vs mutable variables
    val immutable = "cannot change"
    var mutable = "can change"
    mutable = "changed"
    
    // Constructor calls with refinements
    val person = Person("Alice", 30)
    
    // When expression (pattern matching)
    when (person.age) {
        in 0..12 -> println("Child")
        in 13..19 -> println("Teenager")
        else -> println("Adult")
    }
    
    // Nullable types
    val nullable: String? = null
    val length = nullable?.length ?: 0
    
    // Extension functions
    fun String.isPalindrome(): Boolean {
        return this == this.reversed()
    }
    
    println("racecar".isPalindrome())
}

// Class with different function types
class Calculator {
    // Regular method
    fun add(a: Int, b: Int): Int = a + b
    
    // Property with getter/setter
    var result: Int = 0
        set(value) {
            field = if (value >= 0) value else 0
        }
    
    // Companion object (similar to static)
    companion object {
        const val PI = 3.14159
        fun createDefault() = Calculator()
    }
}

// Sealed class for algebraic data types
sealed class Result<T> {
    data class Success<T>(val value: T) : Result<T>()
    data class Error<T>(val message: String) : Result<T>()
}

// Async/coroutine function
suspend fun fetchData(): String {
    delay(1000)
    return "Data fetched"
}