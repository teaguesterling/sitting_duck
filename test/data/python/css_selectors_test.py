# Test fixture for CSS selector coverage
# Designed with deterministic structure for selector testing

class Animal:
    def __init__(self, name):
        self.name = name

    def speak(self):
        return "..."

    def move(self):
        return "moving"


class Dog(Animal):
    def speak(self):
        return "woof"

    def fetch(self, item):
        if item:
            return item
        return None


class Cat(Animal):
    def speak(self):
        return "meow"

    def purr(self):
        pass


def make_animal(kind):
    if kind == "dog":
        return Dog("Rex")
    elif kind == "cat":
        return Cat("Whiskers")
    return Animal("Unknown")


def process_animals(animals):
    for animal in animals:
        print(animal.speak())
        if isinstance(animal, Dog):
            animal.fetch("ball")


def main():
    dog = make_animal("dog")
    cat = make_animal("cat")
    process_animals([dog, cat])
