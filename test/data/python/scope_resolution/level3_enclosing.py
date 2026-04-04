# Level 3: Enclosing scope / closure resolution
# Inner references should resolve to outer definitions

MAX_RETRIES = 3

def outer():
    count = 0

    def inner():
        # count should resolve to outer's count (enclosing scope)
        # MAX_RETRIES should resolve to module-level (2 scopes up)
        return count < MAX_RETRIES

    return inner()

# Global/nonlocal
total = 0

def accumulate(value):
    global total
    total += value
    return total
