#!/usr/bin/env bash
# Negative control for the conformance kit's CALL-NAMED check.
#
# bash `$(...)` command substitution is the known-legitimate unnamed-call shape
# (issue #91: "no single callee exists"). A language forced to DECLARE call
# naming while pointed here must FAIL the check. If it passes, the check cannot
# detect an unnamed call node and the run is void.
#
# CORRECTED (#140a). This used to claim the file "deliberately contains no plain
# `cmd arg` command either". That was never true. `a=$(date +%s)` parses as a
# command_substitution WRAPPING a `command`, so the three lines below have
# always produced 3 `command` nodes beside the 3 substitutions. The claim read
# as true only because `command` was unnamed as well.
#
# #140a names those 3 commands, so this fixture is now 3 unnamed of 6 rather
# than 6 of 6. IT STILL FAILS THE CHECK, which is the only property this
# control needs: the 3 `$(...)` nodes stay unnamed, because a substitution has
# no single callee. Do not add a dynamically-named command to "restore" 6 of 6
# -- the count is measured in test/sql/conformance/call_naming_conformance.test
# and the control is judged by failing, not by its ratio.
a=$(date +%s)
b=$(hostname)
c=$(echo "$a$b")
