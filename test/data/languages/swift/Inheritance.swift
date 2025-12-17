/// Swift Inheritance test file for native extraction validation

// Protocol (Swift's interface)
protocol Runnable {
    func run()
}

// Protocol extending another
protocol Runner: Runnable {
    func sprint()
}

// Protocol with multiple inheritance
protocol Athlete: Runner, CustomStringConvertible {
    func compete()
}

// Base class
class Animal {
    var name: String

    init(name: String) {
        self.name = name
    }

    func makeSound() -> String {
        return ""
    }
}

// Class extending another class
class Dog: Animal {
    override func makeSound() -> String {
        return "Woof!"
    }
}

// Class with extends and protocol conformance
class Pet: Dog, Runnable {
    func run() {
        print("\(name) is running...")
    }
}

// Class with multiple protocol conformance
class ServiceAnimal: Dog, Runnable, CustomStringConvertible {
    func run() {
        print("\(name) is running for service!")
    }

    var description: String {
        return "ServiceAnimal: \(name)"
    }
}

// Final class
final class Bulldog: Dog {
    override func makeSound() -> String {
        return "Gruff!"
    }
}

// Struct with protocol conformance
struct Point: Equatable, Hashable {
    var x: Double
    var y: Double
}

// Enum with protocol conformance
enum DogBreed: String, CaseIterable {
    case labrador = "Labrador"
    case bulldog = "Bulldog"
    case poodle = "Poodle"
}

// Actor (Swift concurrency)
actor DataManager: CustomStringConvertible {
    var data: [String] = []

    var description: String {
        return "DataManager with \(data.count) items"
    }
}

// Generic class with constraints
class Container<T: Equatable>: CustomStringConvertible {
    var items: [T] = []

    var description: String {
        return "Container with \(items.count) items"
    }
}
