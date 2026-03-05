def used_func():
    return 1
def unused_func():
    return 2
def main():
    result = used_func()
    return result
class UsedClass:
    pass
class UnusedClass:
    pass
obj = UsedClass()
def __init__(self):
    pass
