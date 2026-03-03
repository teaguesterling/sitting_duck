// Dart anonymous function assignment examples for testing FIND_ASSIGNMENT_TARGET

void main() {
  // Function expression assigned to variable
  var greet = (String name) {
    return 'Hello, $name!';
  };

  // Lambda expression assigned to variable
  var square = (int x) => x * x;

  // Function expression with final
  final add = (int a, int b) {
    return a + b;
  };

  // Lambda expression with type annotation
  int Function(int) doubler = (x) => x * 2;

  // Anonymous function used inline (no name expected)
  var items = [3, 1, 2];
  items.sort((a, b) => a.compareTo(b));
}
