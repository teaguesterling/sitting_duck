# Simple Python test file for AST parsing

def hello_world():
    """A simple hello world function"""
    print("Hello, World!")

class Calculator:
    """A simple calculator class"""
    
    def __init__(self):
        self.result = 0
    
    def add(self, x, y):
        """Add two numbers"""
        return x + y
    
    def multiply(self, x, y):
        """Multiply two numbers"""
        return x * y

def main():
    calc = Calculator()
    result = calc.add(5, 3)
    print(f"5 + 3 = {result}")
    
    result = calc.multiply(4, 7)
    print(f"4 * 7 = {result}")

if __name__ == "__main__":
    main()