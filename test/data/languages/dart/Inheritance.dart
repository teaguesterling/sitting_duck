/// Dart Inheritance test file for native extraction validation

// Simple abstract class
abstract class Animal {
  String name;

  Animal(this.name);

  String makeSound();
}

// Class extending another class
class Dog extends Animal {
  Dog(String name) : super(name);

  @override
  String makeSound() => 'Woof!';
}

// Mixin definition
mixin Loggable {
  void log(String message) {
    print('[LOG] $message');
  }
}

// Interface (implicit in Dart - any class can be an interface)
abstract class Runnable {
  void run();
}

// Class with extends and implements
class Pet extends Dog implements Runnable {
  Pet(String name) : super(name);

  @override
  void run() {
    print('$name is running...');
  }
}

// Class with extends, implements, and with (mixin)
class ServiceAnimal extends Dog with Loggable implements Runnable {
  ServiceAnimal(String name) : super(name);

  @override
  void run() {
    log('$name is running for service!');
  }
}

// Sealed class (Dart 3.0+)
sealed class Shape {
  double get area;
}

// Class extending sealed class
class Circle extends Shape {
  final double radius;
  Circle(this.radius);

  @override
  double get area => 3.14159 * radius * radius;
}

// Enum with mixin
enum DogBreed with Loggable {
  labrador,
  bulldog,
  poodle;

  void describe() {
    log('Breed: $name');
  }
}

// Extension type (Dart 3.3+)
extension type Meters(double value) {
  double toFeet() => value * 3.28084;
}

// Mixin with on clause
mixin Runner on Animal {
  void sprint() {
    print('$name is sprinting!');
  }
}
