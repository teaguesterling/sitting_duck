def simple():
    return 1
def with_loop(n):
    for i in range(n):
        print(i)
    return n
def with_conditional(x):
    if x > 0:
        return 1
    else:
        return -1
def complex_func(x):
    if x > 0:
        for i in range(x):
            if i % 2 == 0:
                print(i)
    else:
        while x < 0:
            x += 1
    return x
