CONST = 1

class Shape:
    def area(self):
        return 0

    class Inner:
        def deep(self):
            return 1

def top(x: int, y: int) -> int:
    return x + y
