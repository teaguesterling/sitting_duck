# Python weird operators

# Walrus operator (assignment expression)
if (n := len([1, 2, 3])) > 2:
    print(f"List has {n} items")

# Matrix multiplication operator
import numpy as np
A = np.array([[1, 2], [3, 4]])
B = np.array([[5, 6], [7, 8]])
C = A @ B  # Matrix multiplication

# Floor division
result = 17 // 5  # = 3

# Power operator
squared = 2 ** 8  # = 256

# Membership operators
if 'x' in 'xyz':
    pass

if 5 not in [1, 2, 3]:
    pass

# Identity operators
a = [1, 2, 3]
b = a
if a is b:
    pass

if a is not None:
    pass

# Bitwise operators
flags = 0b1010 | 0b0101  # OR
masked = 0b1111 & 0b1010  # AND
flipped = ~0b1010         # NOT
shifted = 1 << 4          # Left shift

# Unpacking operators
def func(*args, **kwargs):
    pass

list1 = [1, 2, 3]
dict1 = {'a': 1}
func(*list1, **dict1)

# Slice operator
lst = [1, 2, 3, 4, 5]
subset = lst[1:3]
reversed = lst[::-1]