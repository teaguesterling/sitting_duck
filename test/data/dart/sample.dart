// Sample Dart file for testing AST parsing
import 'dart:async';
import 'dart:math' as math;

/// A simple Point class with x and y coordinates
class Point {
  final double x;
  final double y;

  /// Creates a new Point with given coordinates
  const Point(this.x, this.y);

  /// Named constructor for origin point
  Point.origin() : x = 0, y = 0;

  /// Factory constructor for creating from map
  factory Point.fromMap(Map<String, double> map) {
    return Point(map['x'] ?? 0, map['y'] ?? 0);
  }

  /// Calculate distance from another point
  double distanceTo(Point other) {
    final dx = x - other.x;
    final dy = y - other.y;
    return math.sqrt(dx * dx + dy * dy);
  }

  /// Add two points
  Point operator +(Point other) => Point(x + other.x, y + other.y);

  @override
  String toString() => 'Point($x, $y)';
}

/// Color enum with RGB values
enum Color {
  red,
  green,
  blue;

  int get rgbValue {
    return switch (this) {
      Color.red => 0xFF0000,
      Color.green => 0x00FF00,
      Color.blue => 0x0000FF,
    };
  }
}

/// Shape abstract class with sealed modifier
sealed class Shape {
  double get area;
}

/// Circle implementation
class Circle extends Shape {
  final double radius;
  Circle(this.radius);

  @override
  double get area => math.pi * radius * radius;
}

/// Rectangle implementation
class Rectangle extends Shape {
  final double width;
  final double height;
  Rectangle(this.width, this.height);

  @override
  double get area => width * height;
}

/// Mixin for printable objects
mixin Printable {
  void printInfo() {
    print(toString());
  }
}

/// Extension on String
extension StringExtension on String {
  String capitalize() {
    if (isEmpty) return this;
    return '${this[0].toUpperCase()}${substring(1)}';
  }
}

/// Generic container class
class Container<T> {
  T? _value;

  T? get value => _value;
  set value(T? newValue) => _value = newValue;

  bool get isEmpty => _value == null;
}

/// Type alias for callback function
typedef StringCallback = void Function(String);

/// Main entry point
Future<void> main() async {
  // Variable declarations
  var message = 'Hello, Dart!';
  final pi = 3.14159;
  const maxSize = 100;
  late String lateVar;

  // Null safety
  String? nullableString;
  String nonNullable = nullableString ?? 'default';

  // Control flow
  if (message.isNotEmpty) {
    print(message);
  } else {
    print('Empty message');
  }

  // For loop
  for (var i = 0; i < 5; i++) {
    if (i == 2) continue;
    if (i == 4) break;
    print(i);
  }

  // For-in loop
  final numbers = [1, 2, 3, 4, 5];
  for (final num in numbers) {
    print(num);
  }

  // While loop
  var counter = 0;
  while (counter < 3) {
    counter++;
  }

  // Do-while loop
  do {
    counter--;
  } while (counter > 0);

  // Switch expression
  final colorName = switch (Color.red) {
    Color.red => 'Red',
    Color.green => 'Green',
    Color.blue => 'Blue',
  };

  // Pattern matching
  final point = Point(3, 4);
  final (x, y) = (point.x, point.y);

  // Try-catch-finally
  try {
    await fetchData();
  } on FormatException catch (e) {
    print('Format error: $e');
  } catch (e, stackTrace) {
    print('Error: $e');
    print(stackTrace);
  } finally {
    print('Cleanup complete');
  }

  // Cascade notation
  final list = <int>[]
    ..add(1)
    ..add(2)
    ..add(3);

  // Collection literals
  final listLiteral = [1, 2, 3];
  final setLiteral = {1, 2, 3};
  final mapLiteral = {'key': 'value', 'count': 42};

  // Spread operator
  final combined = [...listLiteral, 4, 5];

  // Records
  final record = (name: 'Alice', age: 30);
  final (name: userName, age: userAge) = record;

  // Initialize late variable
  lateVar = 'Initialized';
  print(lateVar);
}

/// Async function example
Future<String> fetchData() async {
  await Future.delayed(Duration(seconds: 1));
  return 'Data fetched';
}

/// Stream example
Stream<int> countStream(int max) async* {
  for (var i = 0; i < max; i++) {
    yield i;
  }
}

/// Generator function
Iterable<int> naturals(int n) sync* {
  var k = 0;
  while (k < n) {
    yield k++;
  }
}

/// Private function (starts with underscore)
void _privateHelper() {
  print('This is private');
}

/// Assert example
void validatePositive(int value) {
  assert(value > 0, 'Value must be positive');
}
