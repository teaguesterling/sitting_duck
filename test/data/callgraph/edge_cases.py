# Fixture for :calls / :called-by SCOPE-AWARENESS edge cases (#146).
#
# :calls / :called-by bind a call to its IMMEDIATE enclosing function, not to
# every lexical ancestor. Each call is annotated with the scope it must be
# attributed to. The cases are chosen so that a CONTAINMENT implementation --
# the bug #146 describes -- gives a different answer from a scope-aware one,
# except where marked CONTROL, which guards against over-correcting.

def direct_and_nested():
    both()                              # scope: direct_and_nested
    def inner_same():
        both()                          # scope: inner_same  (same callee, deeper)
    return inner_same

def level1():
    def level2():
        def level3():
            deep()                      # scope: level3  (three levels down)
        return level3
    return level2

class Holder:
    def method(self):
        from_method()                   # scope: method  (a method is a function scope)

    def method_lambda(self):
        return lambda: from_lambda()    # scope: the lambda, NOT method

def recurse(n):
    if n:
        recurse(n - 1)                  # scope: recurse  (self-call)
    return n

def twin_a():
    shared()                            # scope: twin_a

def twin_b():
    shared()                            # scope: twin_b  (same callee, sibling scope)

def with_comprehension():
    return [each(x) for x in range(3)]  # scope: with_comprehension

at_module_level()                       # scope: NONE -- no enclosing function
