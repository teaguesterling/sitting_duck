#!/usr/bin/env bash
# Negative control for the #89 "can't answer -> raise, never silent empty" guard
# on callee-name binding (test/sql/bugs/issue_88_89_callee_name_and_guards.test).
#
# WHY THIS FILE EXISTS. The guard used to be proven with
# test/data/call_extraction/calls.sh, whose `date` and `echo` commands were
# unnamed. #140a names them, so that source became ANSWERABLE and could no
# longer show the guard raising. A guard with no source that triggers it is not
# a tested guard, so the shape moved here rather than being deleted.
#
# EVERY call node below is unnamed, and each for a reason that survives #140a:
# a command whose name is a parameter expansion or a string is resolved at run
# time, so there is no name to bind. `$(...)` has no single callee at all (#91).
# Python returns "" for `fns[k]()` and JavaScript for `obj[k]()`, so this is the
# house answer for a dynamic callee rather than a bash gap.
#
# Do NOT add a literal `cmd arg` command here: one named call node satisfies the
# guard's EXISTS and the guard stops raising, which would silently void this
# control.
$cmd --flag
"$prog" arg
result=$($runner)
