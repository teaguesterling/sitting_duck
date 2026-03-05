MODULE_VAR = 100

class OuterClass:
    class_var = 10
    def method(self):
        local_var = 1
        def nested():
            inner_var = 2
            return inner_var
        return nested()

def standalone():
    x = 42
    return x
