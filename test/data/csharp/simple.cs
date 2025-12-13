// Simple C# example demonstrating modern language features
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;

namespace SittingDuck.Examples
{
    // Interface definition
    public interface IGreeter
    {
        string Greet(string name);
    }

    // Record type (C# 9+)
    public record Person(string Name, int Age);

    // Main class with various features
    public class Calculator : IGreeter
    {
        // Properties
        public int Result { get; private set; }

        // Auto-implemented property with initializer
        public string Version { get; } = "1.0.0";

        // Constructor
        public Calculator()
        {
            Result = 0;
        }

        // Method with expression body
        public int Add(int a, int b) => a + b;

        // Traditional method
        public int Subtract(int a, int b)
        {
            return a - b;
        }

        // Async method
        public async Task<string> FetchDataAsync()
        {
            await Task.Delay(100);
            return "Data fetched";
        }

        // Interface implementation
        public string Greet(string name)
        {
            return $"Hello, {name}!";
        }

        // Static method
        public static Calculator Create() => new Calculator();

        // Method with pattern matching
        public string Classify(object obj)
        {
            return obj switch
            {
                int i when i > 0 => "Positive integer",
                int i when i < 0 => "Negative integer",
                string s => $"String: {s}",
                null => "Null value",
                _ => "Unknown type"
            };
        }
    }

    // Enum definition
    public enum Status
    {
        Active,
        Inactive,
        Pending
    }

    // Struct definition
    public struct Point
    {
        public int X { get; set; }
        public int Y { get; set; }

        public Point(int x, int y)
        {
            X = x;
            Y = y;
        }

        public double Distance() => Math.Sqrt(X * X + Y * Y);
    }

    // Generic class
    public class Container<T>
    {
        private readonly List<T> _items = new();

        public void Add(T item) => _items.Add(item);

        public T? GetFirst() => _items.FirstOrDefault();

        public IEnumerable<T> GetAll() => _items;
    }

    // Program entry point
    public class Program
    {
        public static void Main(string[] args)
        {
            var calc = new Calculator();
            Console.WriteLine(calc.Add(5, 3));

            // Using LINQ
            var numbers = new[] { 1, 2, 3, 4, 5 };
            var evens = numbers.Where(n => n % 2 == 0).ToList();

            // Pattern matching with switch
            foreach (var num in numbers)
            {
                var result = num switch
                {
                    1 => "one",
                    2 => "two",
                    _ => "other"
                };
                Console.WriteLine(result);
            }

            // Null-conditional and null-coalescing
            string? nullable = null;
            var length = nullable?.Length ?? 0;

            // Using record
            var person = new Person("Alice", 30);
            Console.WriteLine(person);
        }
    }
}
