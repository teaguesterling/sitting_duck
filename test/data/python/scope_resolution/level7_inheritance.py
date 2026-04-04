# Level 7: Inheritance and method resolution order (MRO)
# Methods may come from parent classes

class Animal:
    def __init__(self, name):
        self.name = name

    def speak(self):
        return f"{self.name} makes a sound"

    def describe(self):
        return f"Animal: {self.name}"


class Dog(Animal):
    def __init__(self, name, breed):
        super().__init__(name)
        self.breed = breed

    def speak(self):
        # Overrides Animal.speak
        return f"{self.name} barks"

    def fetch(self):
        return f"{self.name} fetches"


class Puppy(Dog):
    def __init__(self, name, breed):
        super().__init__(name, breed)

    # Does NOT override speak — inherits from Dog
    # Does NOT override describe — inherits from Animal (skips Dog)
    # Does NOT override fetch — inherits from Dog

    def play(self):
        return f"{self.name} plays"


# Resolution chain:
# puppy = Puppy("Rex", "Lab")
# puppy.speak()    → Dog.speak (MRO: Puppy → Dog → Animal)
# puppy.describe() → Animal.describe (MRO: Puppy → Dog → Animal)
# puppy.fetch()    → Dog.fetch
# puppy.play()     → Puppy.play
# puppy.name       → Animal.__init__'s self.name (via super chain)
def demo():
    puppy = Puppy("Rex", "Lab")
    print(puppy.speak())      # Dog.speak
    print(puppy.describe())   # Animal.describe
    print(puppy.fetch())      # Dog.fetch
    print(puppy.play())       # Puppy.play
