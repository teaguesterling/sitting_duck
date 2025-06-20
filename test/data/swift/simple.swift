// Simple Swift test file demonstrating various language constructs

import Foundation
import Combine

// MARK: - Type Definitions

// Protocol definition
protocol Drawable {
    var area: Double { get }
    func draw()
    func move(to point: CGPoint)
}

// Protocol with associated type
protocol Container {
    associatedtype Item
    var count: Int { get }
    mutating func append(_ item: Item)
    subscript(i: Int) -> Item { get }
}

// Enum with associated values and raw values
enum NetworkError: Error, CaseIterable {
    case noConnection
    case timeout(seconds: Int)
    case serverError(code: Int, message: String)
    case invalidResponse
    
    var localizedDescription: String {
        switch self {
        case .noConnection:
            return "No internet connection"
        case .timeout(let seconds):
            return "Request timed out after \(seconds) seconds"
        case .serverError(let code, let message):
            return "Server error \(code): \(message)"
        case .invalidResponse:
            return "Invalid server response"
        }
    }
}

// Enum with raw values
enum Priority: Int, CaseIterable {
    case low = 1
    case medium = 2
    case high = 3
    case critical = 4
    
    var description: String {
        switch self {
        case .low: return "Low Priority"
        case .medium: return "Medium Priority"
        case .high: return "High Priority"
        case .critical: return "Critical Priority"
        }
    }
}

// Struct implementing protocols
struct Rectangle: Drawable {
    var width: Double
    var height: Double
    private(set) var position: CGPoint = .zero
    
    // Computed property
    var area: Double {
        return width * height
    }
    
    var perimeter: Double {
        get {
            return 2 * (width + height)
        }
    }
    
    // Initializers
    init(width: Double, height: Double) {
        self.width = width
        self.height = height
    }
    
    init(square side: Double) {
        self.init(width: side, height: side)
    }
    
    // Methods
    func draw() {
        print("Drawing rectangle: \(width) x \(height) at \(position)")
    }
    
    mutating func move(to point: CGPoint) {
        position = point
    }
    
    // Static method
    static func random() -> Rectangle {
        let width = Double.random(in: 1...100)
        let height = Double.random(in: 1...100)
        return Rectangle(width: width, height: height)
    }
}

// Generic struct implementing Container protocol
struct Stack<Element>: Container {
    private var items: [Element] = []
    
    var count: Int {
        return items.count
    }
    
    var isEmpty: Bool {
        return items.isEmpty
    }
    
    mutating func append(_ item: Element) {
        items.append(item)
    }
    
    mutating func pop() -> Element? {
        return items.isEmpty ? nil : items.removeLast()
    }
    
    subscript(i: Int) -> Element {
        return items[i]
    }
    
    // Generic method with constraints
    func filter<T>(_ transform: (Element) -> T?) -> [T] {
        return items.compactMap(transform)
    }
}

// Class with inheritance
class Shape {
    var name: String
    private var _id: UUID = UUID()
    
    // Property observers
    var color: UIColor = .black {
        willSet {
            print("Color will change from \(color) to \(newValue)")
        }
        didSet {
            print("Color changed from \(oldValue) to \(color)")
        }
    }
    
    init(name: String) {
        self.name = name
    }
    
    // Method to be overridden
    func describe() -> String {
        return "Shape named \(name) with color \(color)"
    }
    
    // Final method (cannot be overridden)
    final func getId() -> UUID {
        return _id
    }
    
    deinit {
        print("Shape \(name) is being deallocated")
    }
}

// Subclass with method overriding
class Circle: Shape, Drawable {
    var radius: Double
    private(set) var center: CGPoint = .zero
    
    // Computed property
    var area: Double {
        return Double.pi * radius * radius
    }
    
    var diameter: Double {
        get { return radius * 2 }
        set { radius = newValue / 2 }
    }
    
    init(name: String, radius: Double) {
        self.radius = radius
        super.init(name: name)
    }
    
    // Convenience initializer
    convenience init(radius: Double) {
        self.init(name: "Circle", radius: radius)
    }
    
    override func describe() -> String {
        return super.describe() + " with radius \(radius)"
    }
    
    func draw() {
        print("Drawing circle with radius \(radius) at \(center)")
    }
    
    func move(to point: CGPoint) {
        center = point
    }
}

// Actor for concurrent programming
actor BankAccount {
    private var balance: Double = 0.0
    private let accountNumber: String
    
    init(accountNumber: String, initialBalance: Double = 0.0) {
        self.accountNumber = accountNumber
        self.balance = initialBalance
    }
    
    func deposit(_ amount: Double) async -> Double {
        guard amount > 0 else { return balance }
        balance += amount
        return balance
    }
    
    func withdraw(_ amount: Double) async throws -> Double {
        guard amount > 0 else { throw NetworkError.invalidResponse }
        guard balance >= amount else {
            throw NetworkError.serverError(code: 400, message: "Insufficient funds")
        }
        balance -= amount
        return balance
    }
    
    func getBalance() async -> Double {
        return balance
    }
}

// MARK: - Extensions

extension String {
    var isEmail: Bool {
        let emailRegex = "[A-Z0-9a-z._%+-]+@[A-Za-z0-9.-]+\\.[A-Za-z]{2,}"
        let emailPredicate = NSPredicate(format: "SELF MATCHES %@", emailRegex)
        return emailPredicate.evaluate(with: self)
    }
    
    func truncated(to length: Int, trailing: String = "...") -> String {
        guard self.count > length else { return self }
        return String(self.prefix(length)) + trailing
    }
}

extension Array {
    var isNotEmpty: Bool {
        return !isEmpty
    }
    
    func chunked(into size: Int) -> [[Element]] {
        return stride(from: 0, to: count, by: size).map {
            Array(self[$0..<Swift.min($0 + size, count)])
        }
    }
}

// MARK: - Property Wrappers

@propertyWrapper
struct Clamped<T: Comparable> {
    private var value: T
    private let range: ClosedRange<T>
    
    var wrappedValue: T {
        get { value }
        set { value = min(max(range.lowerBound, newValue), range.upperBound) }
    }
    
    init(wrappedValue: T, _ range: ClosedRange<T>) {
        self.range = range
        self.value = min(max(range.lowerBound, wrappedValue), range.upperBound)
    }
}

@propertyWrapper
struct UserDefault<T> {
    let key: String
    let defaultValue: T
    
    var wrappedValue: T {
        get {
            return UserDefaults.standard.object(forKey: key) as? T ?? defaultValue
        }
        set {
            UserDefaults.standard.set(newValue, forKey: key)
        }
    }
    
    init(_ key: String, defaultValue: T) {
        self.key = key
        self.defaultValue = defaultValue
    }
}

// MARK: - Main Classes and Functions

class TaskManager {
    @UserDefault("username", defaultValue: "Guest")
    var username: String
    
    @Clamped(0...100)
    var progress: Int = 0
    
    private var tasks: [Task] = []
    private lazy var dateFormatter: DateFormatter = {
        let formatter = DateFormatter()
        formatter.dateStyle = .medium
        formatter.timeStyle = .short
        return formatter
    }()
    
    struct Task {
        let id: UUID = UUID()
        var title: String
        var isCompleted: Bool = false
        var priority: Priority = .medium
        var dueDate: Date?
        
        mutating func complete() {
            isCompleted = true
        }
        
        var description: String {
            let status = isCompleted ? "✅" : "⭕"
            let due = dueDate.map { " (due: \($0))" } ?? ""
            return "\(status) \(title) [\(priority.description)]\(due)"
        }
    }
    
    func addTask(_ title: String, priority: Priority = .medium, dueDate: Date? = nil) {
        let task = Task(title: title, priority: priority, dueDate: dueDate)
        tasks.append(task)
    }
    
    func completeTask(at index: Int) -> Bool {
        guard tasks.indices.contains(index) else { return false }
        tasks[index].complete()
        updateProgress()
        return true
    }
    
    private func updateProgress() {
        let completedCount = tasks.filter { $0.isCompleted }.count
        progress = tasks.isEmpty ? 0 : Int(Double(completedCount) / Double(tasks.count) * 100)
    }
    
    func getTasks(sortedBy sortCriteria: (Task, Task) -> Bool) -> [Task] {
        return tasks.sorted(by: sortCriteria)
    }
    
    // Method with throwing and async
    func syncTasks() async throws {
        print("Syncing tasks for user: \(username)")
        
        // Simulate network request
        try await Task.sleep(nanoseconds: 1_000_000_000) // 1 second
        
        // Simulate potential failure
        if Bool.random() {
            throw NetworkError.timeout(seconds: 5)
        }
        
        print("Tasks synced successfully")
    }
}

// MARK: - Global Functions

// Function with multiple parameter types
func processData<T: Codable>(
    _ data: [T],
    filter predicate: (T) -> Bool = { _ in true },
    transform: (T) throws -> T,
    completion: @escaping (Result<[T], Error>) -> Void
) {
    DispatchQueue.global(qos: .background).async {
        do {
            let filtered = data.filter(predicate)
            let transformed = try filtered.map(transform)
            
            DispatchQueue.main.async {
                completion(.success(transformed))
            }
        } catch {
            DispatchQueue.main.async {
                completion(.failure(error))
            }
        }
    }
}

// Function with variadic parameters and inout
func calculateStats(_ numbers: Double..., result: inout (min: Double, max: Double, avg: Double)?) {
    guard !numbers.isEmpty else {
        result = nil
        return
    }
    
    let min = numbers.min()!
    let max = numbers.max()!
    let avg = numbers.reduce(0, +) / Double(numbers.count)
    
    result = (min: min, max: max, avg: avg)
}

// Higher-order function
func performOperation<T, U>(
    on value: T,
    operations: [(T) throws -> T]
) throws -> T {
    return try operations.reduce(value) { result, operation in
        try operation(result)
    }
}

// MARK: - Main Program

@main
struct SimpleApp {
    static func main() async {
        print("Swift Language Test Program")
        print("============================")
        
        // Basic types and literals
        let name: String = "Swift Test"
        let version: Double = 1.0
        let isActive: Bool = true
        let optionalValue: String? = nil
        
        // Collections
        var numbers = [1, 2, 3, 4, 5]
        let squares = numbers.map { $0 * $0 }
        let evens = numbers.filter { $0 % 2 == 0 }
        
        // Dictionary
        let userInfo: [String: Any] = [
            "name": name,
            "version": version,
            "active": isActive,
            "features": ["networking", "persistence", "ui"]
        ]
        
        // Optional binding and nil coalescing
        if let value = optionalValue {
            print("Optional has value: \(value)")
        } else {
            print("Optional is nil, using default: \(optionalValue ?? "default")")
        }
        
        // Guard statement
        guard isActive else {
            print("Application is not active")
            return
        }
        
        // Working with custom types
        let rectangle = Rectangle(width: 10, height: 5)
        let circle = Circle(radius: 7.5)
        let shapes: [Drawable] = [rectangle, circle]
        
        print("\nDrawing shapes:")
        for shape in shapes {
            shape.draw()
            print("Area: \(shape.area)")
        }
        
        // Working with generics
        var stringStack = Stack<String>()
        stringStack.append("first")
        stringStack.append("second")
        stringStack.append("third")
        
        print("\nStack contents:")
        for i in 0..<stringStack.count {
            print("Item \(i): \(stringStack[i])")
        }
        
        // Working with enums
        let errors: [NetworkError] = [
            .noConnection,
            .timeout(seconds: 30),
            .serverError(code: 500, message: "Internal Server Error"),
            .invalidResponse
        ]
        
        print("\nNetwork errors:")
        for error in errors {
            print("- \(error.localizedDescription)")
        }
        
        // Task management example
        let taskManager = TaskManager()
        taskManager.addTask("Learn Swift", priority: .high, dueDate: Date().addingTimeInterval(86400))
        taskManager.addTask("Write tests", priority: .medium)
        taskManager.addTask("Deploy app", priority: .critical, dueDate: Date().addingTimeInterval(172800))
        
        // Complete first task
        if taskManager.completeTask(at: 0) {
            print("Task completed successfully")
        }
        
        // Get sorted tasks
        let sortedTasks = taskManager.getTasks { $0.priority.rawValue > $1.priority.rawValue }
        print("\nTasks (sorted by priority):")
        for (index, task) in sortedTasks.enumerated() {
            print("\(index + 1). \(task.description)")
        }
        
        print("Progress: \(taskManager.progress)%")
        
        // Async operations
        do {
            print("\nAttempting to sync tasks...")
            try await taskManager.syncTasks()
        } catch let error as NetworkError {
            print("Sync failed: \(error.localizedDescription)")
        } catch {
            print("Unexpected error: \(error)")
        }
        
        // Actor usage
        let account = BankAccount(accountNumber: "12345", initialBalance: 1000.0)
        
        do {
            let balance = await account.deposit(200.0)
            print("\nAccount balance after deposit: $\(balance)")
            
            let newBalance = try await account.withdraw(150.0)
            print("Account balance after withdrawal: $\(newBalance)")
        } catch {
            print("Banking operation failed: \(error)")
        }
        
        // String extensions
        let email = "test@example.com"
        let longText = "This is a very long text that needs to be truncated"
        
        print("\nString operations:")
        print("'\(email)' is email: \(email.isEmail)")
        print("Truncated: '\(longText.truncated(to: 20))'")
        
        // Array extensions
        let items = Array(1...10)
        let chunks = items.chunked(into: 3)
        print("Chunked array: \(chunks)")
        
        // Closures and trailing closures
        let transformedNumbers = numbers.enumerated().compactMap { index, value -> String? in
            guard value % 2 == 0 else { return nil }
            return "Even[\(index)]: \(value)"
        }
        
        print("\nTransformed numbers: \(transformedNumbers)")
        
        // Statistical calculation
        var stats: (min: Double, max: Double, avg: Double)?
        calculateStats(1.5, 2.7, 3.1, 4.9, 5.2, result: &stats)
        
        if let stats = stats {
            print("\nStatistics - Min: \(stats.min), Max: \(stats.max), Avg: \(String(format: "%.2f", stats.avg))")
        }
        
        print("\nProgram completed successfully!")
    }
}