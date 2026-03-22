def outer():
    x = 1
    def inner():
        if True:
            return 1
        return 0
    return inner()
