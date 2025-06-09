from typing import List, Dict, Optional, Union, Callable, TypeVar, Generic
from dataclasses import dataclass

# Function with type hints
def add_numbers(x: int, y: int) -> int:
    """Add two integers"""
    return x + y

def process_data(items: List[str], config: Dict[str, any] = None) -> Optional[str]:
    """Process a list of items with optional config"""
    if not items:
        return None
    return items[0]

# Generic function
T = TypeVar('T')
def identity(value: T) -> T:
    return value

# Complex types
def transform_callback(data: List[Dict[str, Union[str, int]]], 
                      callback: Callable[[str], bool]) -> List[str]:
    """Transform data using callback"""
    result = []
    for item in data:
        if callback(str(item.get('name', ''))):
            result.append(str(item))
    return result

# Class with typed methods
@dataclass
class Person:
    name: str
    age: int
    email: Optional[str] = None
    
    def get_display_name(self) -> str:
        return f"{self.name} ({self.age})"
    
    def update_email(self, new_email: str) -> None:
        self.email = new_email
        
    @classmethod
    def from_dict(cls, data: Dict[str, any]) -> 'Person':
        return cls(
            name=data['name'],
            age=data['age'],
            email=data.get('email')
        )

# Async function with types
async def fetch_user_data(user_id: int) -> Optional[Dict[str, any]]:
    """Fetch user data asynchronously"""
    # Simulate async operation
    return {"id": user_id, "name": "User"}

# Generator with types
def number_generator(start: int, end: int) -> Generator[int, None, None]:
    for i in range(start, end):
        yield i