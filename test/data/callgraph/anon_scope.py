# Fixture for the ANONYMOUS-enclosing-scope and MODULE-LEVEL call cases, where
# ast_callees, ast_callers and the ::callees/::callers pseudo-elements provably
# disagree.
#
# WHY THIS DIRECTORY, not test/data/python/ — do not move it back.
# Adding any .py file to test/data/python/ breaks exact-count assertions that
# glob it: multi_file_edge_cases.test's `COUNT(DISTINCT file_path) = 25` and
# core/glob_array_support.test's per-language file counts. Those counts are
# deliberately exact and are guards worth keeping, so the fixture moves rather
# than the expectation. The globs that DO reach this directory
# (`test/data/*/*.py`, `test/data/**/*.py`) were checked and are all `> 0` /
# `> 100` bounds, which adding a file cannot break.
# (callgraph_direct.py stays in test/data/python/ because it predates those
# counts and is already baked into the 25 — moving it would break them too.)
#
# Deliberate shape:
#   - named()            calls named_target()  -> enclosing function has a name
#   - an ANONYMOUS lambda (an argument, so no assignment target to borrow a name
#     from) calls anon_target()
#   - a NAMED lambda (`bound = lambda: ...`) calls bound_target(); sitting_duck
#     infers the name `bound` from the assignment, so this is NOT anonymous
#   - module_target() is called at MODULE level, inside no function at all
#   - twice_target() is called TWICE from one function, to pin call-site grain


def named_target():
    pass


def anon_target():
    pass


def bound_target():
    pass


def module_target():
    pass


def twice_target():
    pass


def named():
    named_target()


def uses_anon_lambda(items):
    # The lambda is an argument: no assignment target, so no name to infer.
    return sorted(items, key=lambda item: anon_target())


bound = lambda: bound_target()


def calls_twice():
    twice_target()
    twice_target()


module_target()
