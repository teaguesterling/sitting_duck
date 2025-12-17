"""Python inheritance test file"""

class Animal:
    """Base animal class"""
    def __init__(self, name):
        self.name = name

    def speak(self):
        pass

class Dog(Animal):
    """Dog inherits from Animal"""
    def speak(self):
        return "Woof!"

class Cat(Animal):
    """Cat inherits from Animal"""
    def speak(self):
        return "Meow!"

class Pet(Dog, Cat):
    """Multiple inheritance example"""
    def __init__(self, name, owner):
        super().__init__(name)
        self.owner = owner
