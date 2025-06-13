class Person
  def initialize(name)
    @name = name
  end

  def greet
    puts "Hello, #{@name}!"
  end

  def self.species
    "Homo sapiens"
  end
end

def say_hello(name)
  person = Person.new(name)
  person.greet
end

say_hello("World")