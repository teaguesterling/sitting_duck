# Fixture for direct (immediate-scope) call-graph pseudo-classes.
# A call inside a lambda / nested function belongs to THAT inner scope, not to
# the outer function (:calls / :called-by are direct — #146/#152/#164).
def outer():
    helper()                       # immediate function: outer
    inner = lambda: helper2()      # helper2's immediate function: the lambda `inner`
    def nested():
        helper3()                  # immediate function: nested
    return inner
