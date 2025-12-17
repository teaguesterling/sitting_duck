package com.example.inheritance;

import java.io.Serializable;

/**
 * Java inheritance test file - demonstrates various inheritance patterns
 */

// Base class with no inheritance
abstract class Animal {
    protected String name;

    public Animal(String name) {
        this.name = name;
    }

    public abstract String speak();
}

// Single inheritance
class Dog extends Animal {
    public Dog(String name) {
        super(name);
    }

    @Override
    public String speak() {
        return "Woof!";
    }
}

// Single inheritance
class Cat extends Animal {
    public Cat(String name) {
        super(name);
    }

    @Override
    public String speak() {
        return "Meow!";
    }
}

// Interface declaration with extends
interface Runnable {
    void run();
}

// Interface extending another interface
interface Runner extends Runnable {
    void sprint();
}

// Interface with multiple extends
interface Athlete extends Runner, Serializable {
    void compete();
}

// Class with extends and implements
class Pet extends Dog implements Serializable {
    private String owner;

    public Pet(String name, String owner) {
        super(name);
        this.owner = owner;
    }
}

// Class with extends and multiple implements
class ServiceAnimal extends Dog implements Runnable, Serializable {
    public ServiceAnimal(String name) {
        super(name);
    }

    @Override
    public void run() {
        System.out.println(name + " is running");
    }
}

// Enum implementing an interface
enum DogBreed implements Serializable {
    LABRADOR,
    POODLE,
    BULLDOG
}

// Functional interface
@FunctionalInterface
interface SoundMaker {
    String makeSound();
}
