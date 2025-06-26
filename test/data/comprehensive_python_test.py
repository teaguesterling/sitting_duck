# Comprehensive Python test file for native context extraction

# Basic function with positional and default parameters
def basic_function(arg1, arg2="default", arg3=None):
    """A basic function with mixed parameters."""
    return arg1 + str(arg2)

# Function with type annotations
def typed_function(name: str, age: int, active: bool = True) -> str:
    """Function with full type annotations."""
    return f"{name} is {age} years old"

# Async function with complex parameters
async def async_process(data: list[str], timeout: float = 5.0, **kwargs) -> dict:
    """Async function with complex type hints."""
    return {"processed": len(data), "timeout": timeout}

# Function with *args and **kwargs
def variadic_function(*args, **kwargs):
    """Function with variadic parameters."""
    return len(args) + len(kwargs)

# Function with complex default values
def complex_defaults(items: list = None, config: dict = {}):
    """Function with complex default values."""
    if items is None:
        items = []
    return len(items)

# Method in a class
class ExampleClass:
    """A class with various method types."""
    
    def __init__(self, name: str, value: int = 0):
        """Constructor with parameters."""
        self.name = name
        self.value = value
    
    def instance_method(self, multiplier: int) -> int:
        """Instance method with parameters."""
        return self.value * multiplier
    
    @classmethod
    def class_method(cls, name: str) -> 'ExampleClass':
        """Class method with decorator."""
        return cls(name)
    
    @staticmethod
    def static_method(x: int, y: int) -> int:
        """Static method with decorator."""
        return x + y
    
    @property
    def computed_value(self) -> int:
        """Property with decorator."""
        return self.value * 2

# Class with inheritance
class ChildClass(ExampleClass):
    """Child class with inheritance."""
    
    def __init__(self, name: str, value: int = 0, extra: str = ""):
        """Child constructor."""
        super().__init__(name, value)
        self.extra = extra

# Function with multiple decorators
@property
@classmethod
def decorated_function(cls):
    """Function with multiple decorators."""
    return "decorated"

# Lambda functions (should be detected as expressions)
lambda_func = lambda x, y=1: x + y

# Nested function
def outer_function(param1):
    """Outer function with nested function."""
    
    def inner_function(param2: str) -> str:
        """Nested function."""
        return param1 + param2
    
    return inner_function

# Generator function
def generator_function(items: list) -> str:
    """Generator function."""
    for item in items:
        yield str(item)

# Variable assignments with type hints
variable_with_type: int = 42
string_variable: str = "hello"
complex_variable: dict[str, list[int]] = {"nums": [1, 2, 3]}

# Global constant
GLOBAL_CONSTANT = "constant_value"