def hello():
    """Say hello"""
    print("Hello, World!")

class MyClass:
    """A test class"""
    def __init__(self):
        self.value = 42
        
    def add(self, x, y):
        return x + y
        
def main():
    obj = MyClass()
    hello()
    
if __name__ == "__main__":
    main()