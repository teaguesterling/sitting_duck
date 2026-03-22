def has_try():
    try:
        risky()
    except:
        pass
def no_try():
    safe()
def has_eval():
    result = eval("1+1")
    return result
def nested_try():
    def inner():
        try:
            x = 1
        except:
            pass
    inner()
