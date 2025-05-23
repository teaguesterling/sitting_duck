@property
@functools.lru_cache(maxsize=128)
def cached_property(self):
    return expensive_computation()

@dataclass
class Point:
    x: float
    y: float
    
async def async_function():
    await something()
    
def generator():
    yield from range(10)