# Level 2: Parameter resolution
# References inside function should resolve to parameters

def greet(name):
    message = f"Hello, {name}"
    return message

def add(a, b):
    result = a + b
    return result

# Default parameter
def connect(host, port=5432):
    url = f"{host}:{port}"
    return url
