# Fixture for the ANONYMOUS-enclosing-scope and MODULE-LEVEL call cases, where
# ast_callees, ast_callers and the ::callees/::callers pseudo-elements provably
# disagree. Lives HERE, not test/data/python/ (#211); must not move back, and any
# line added above the code breaks 10 pinned line numbers — see callgraph_macros.test.
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
