# Negative control for the conformance kit's PARSE harness guard.
#
# Deliberately unparseable Python. A run that reports this file as a clean
# parse means the guard cannot distinguish "parsed fine" from "parsed into
# ERROR nodes", and every per-language verdict in that run is void.
#
# Do NOT "fix" this file. Its brokenness is the assertion.
def (((:
    ???
class 123 !!!
    return return
