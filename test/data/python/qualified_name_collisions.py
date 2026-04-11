# Test fixture for qualified_name collision-only suffix logic.
#
# Exercises cases where multiple definitions in the same scope would
# produce the same bare segment (V[x], F[foo], etc.), so the second
# and subsequent occurrences should get a [N] suffix.

# Two assignments to the same module-level variable.
# Expected: V[counter], V[counter][2]
counter = 0
counter = 1

# Three assignments to another module-level variable.
# Expected: V[total], V[total][2], V[total][3]
total = 0
total = 10
total = 20


def transform_items(items):
    # Two local variables with the same name inside the same function.
    # Expected: F[transform_items] V[result], F[transform_items] V[result][2]
    result = []
    result = [i * 2 for i in items]
    return result


def accumulator():
    # Separate scope — counter resets. First occurrence of "result"
    # in this scope should be unsuffixed again.
    # Expected: F[accumulator] V[result]
    result = 0
    return result


class Container:
    def __init__(self, value):
        # Two assignments with the same RHS identifier produce V[value]
        # and V[value][2] inside the __init__ scope. (Sitting Duck records
        # the identifier on the RHS of self.attr = ident as a definition.)
        # Expected: C[Container] F[__init__] V[value]
        #           C[Container] F[__init__] V[value][2]
        self.a = value
        self.b = value

    def reset(self):
        pass

    def reset(self):  # noqa: overridden intentionally
        # Method redefinition — same class scope, same name.
        # Expected: C[Container] F[reset], C[Container] F[reset][2]
        pass
