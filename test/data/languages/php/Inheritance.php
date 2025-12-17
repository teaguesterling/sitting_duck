<?php
/**
 * PHP Inheritance test file for native extraction validation
 */

// Simple interface
interface Runnable {
    public function run(): void;
}

// Interface extending another
interface Runner extends Runnable {
    public function sprint(): void;
}

// Interface extending multiple interfaces
interface Athlete extends Runner, Serializable {
    public function compete(): void;
}

// Abstract base class
abstract class Animal {
    protected string $name;

    abstract public function makeSound(): string;
}

// Class extending another class
class Dog extends Animal {
    public function makeSound(): string {
        return "Woof!";
    }
}

// Class extending and implementing
class Pet extends Dog implements Serializable {
    public function serialize(): string {
        return serialize($this->name);
    }

    public function unserialize(string $data): void {
        $this->name = unserialize($data);
    }
}

// Class with multiple implements
class ServiceAnimal extends Dog implements Runnable, Serializable {
    public function run(): void {
        echo "Running...";
    }

    public function serialize(): string {
        return serialize($this->name);
    }

    public function unserialize(string $data): void {
        $this->name = unserialize($data);
    }
}

// Final class
final class Bulldog extends Dog {
    public function makeSound(): string {
        return "Gruff!";
    }
}

// Trait (PHP-specific)
trait Loggable {
    public function log(string $message): void {
        echo "[LOG] " . $message;
    }
}

// Enum implementing interface (PHP 8.1+)
enum DogBreed: string implements Serializable {
    case LABRADOR = 'labrador';
    case BULLDOG = 'bulldog';
    case POODLE = 'poodle';

    public function serialize(): string {
        return $this->value;
    }

    public function unserialize(string $data): void {
        // Enums are immutable
    }
}
