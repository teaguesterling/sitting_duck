# Python lambda assignment examples for testing FIND_ASSIGNMENT_TARGET

# Simple lambda assigned to variable
square = lambda x: x * x

# Lambda with multiple parameters
add = lambda x, y: x + y

# Lambda with default parameter
greet = lambda name="World": f"Hello, {name}!"

# Lambda assigned with walrus operator
result = (filtered := lambda xs: [x for x in xs if x > 0])

# Lambda not assigned (used inline) - should have no name
sorted_items = sorted([3, 1, 2], key=lambda x: x)
