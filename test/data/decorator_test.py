@staticmethod
def simple_decorated():
    pass

@classmethod  
def class_decorated():
    pass

@property
def property_decorated():
    pass

@app.route('/api/users')
def route_decorated():
    pass

@app.route('/api/data', methods=['GET', 'POST'])
def complex_route():
    pass

@dataclass
class DecoratedClass:
    name: str

@validator('field_name')
def validation_function(cls, v):
    return v

@functools.lru_cache(maxsize=128)
def cached_function(param1: int, param2: str = "default") -> str:
    return f"{param1}_{param2}"

@pytest.mark.parametrize("input,expected", [
    (1, "one"),
    (2, "two")
])
def test_function(input, expected):
    pass