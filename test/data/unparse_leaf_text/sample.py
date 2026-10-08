# leading comment
import os


def greet(name: str) -> str:
    """Docstring."""
    # inner comment
    prefix = "hello"
    count = 42
    ratio = 3.5
    return f"{prefix} {name} {count} {ratio}"


print(greet(os.name))
