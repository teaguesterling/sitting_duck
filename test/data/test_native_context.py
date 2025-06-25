def simple_function(param1, param2="default"):
    """A simple function with parameters."""
    return param1 + param2

async def async_function(data: str, count: int = 5) -> str:
    """An async function with type annotations."""
    return data * count

class BaseClass:
    pass

class MyClass(BaseClass):
    """A class with inheritance."""
    
    def __init__(self, name: str):
        self.name = name
    
    def method_with_types(self, value: int, optional: str = None) -> bool:
        return value > 0

@property
def decorated_function():
    """Function with decorator."""
    pass

# Variable with type annotation
my_var: int = 42