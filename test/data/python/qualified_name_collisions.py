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
        # These define self.a and self.b, NOT `value`, which is the
        # parameter and is read twice. This comment used to claim the RHS
        # identifier was recorded as a definition; that was #273 and is
        # fixed. The method-scope collision these lines used to supply is
        # asserted inline in test/sql/qualified_name.test (Test 19).
        self.a = value
        self.b = value

    def reset(self):
        pass

    def reset(self):  # noqa: overridden intentionally
        # Method redefinition — same class scope, same name.
        # Expected: C[Container] F[reset], C[Container] F[reset][2]
        pass
