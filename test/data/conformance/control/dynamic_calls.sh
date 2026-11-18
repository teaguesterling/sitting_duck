#!/usr/bin/env bash
# Negative control for the #89 "can't answer -> raise, never silent empty" guard
# on callee-name binding (test/sql/bugs/issue_88_89_callee_name_and_guards.test
# and test/sql/conformance/no_silent_empty.test).
#
# WHY THIS FILE EXISTS. The guard was first proven with
# test/data/call_extraction/calls.sh, whose `date` and `echo` commands were
# unnamed. #140a named them, so that source became ANSWERABLE and could no
# longer show the guard raising. A guard with no source that triggers it is not
# a tested guard, so the shape moved here rather than being deleted.
#
# EVERY call node below must be unnamed, or this control silently stops being
# one: a single named call node satisfies the guard's EXISTS and the guard stops
# raising.
#
# THE SET OF UNNAMED SHAPES NARROWED ON 2026-11-18, and this file was rewritten
# because of it. Teague ruled that a bash command named by a VARIABLE is named as
# written, so `$cmd --flag` now reports `$cmd` and `$runner` reports `$runner`.
# Both were in this file and both had to go. What remains are the two shapes
# that are still unnamed, for reasons the ruling does not touch:
#
#   "$prog" arg        a STRING in command position. Its contents are not
#                      knowable, and it is not a variable-expansion node.
#   $("$runner")       a COMMAND SUBSTITUTION has no single callee at all (#91),
#                      and its inner command is itself a string, so neither the
#                      outer nor the inner node carries a name. A BARE
#                      $(...) inner callee like $($runner) would now name the
#                      inner one.
#
# DO NOT ADD, because each would void this control:
#   * a literal `cmd arg`           -- named by the `word` path
#   * a bare `$cmd` or `${cmd}`     -- named as written since 2026-11-18
#   * $($runner) or $(cmd)          -- the INNER command gets named
#
# Measured after each change: `SELECT count(*) FILTER (WHERE name <> '')` over
# this file's `command` rows must be 0. test/sql/languages/bash_variable_named_command.test
# section 3 pins the two surviving shapes directly, so a later change that names
# them fails there as well as here.
"$prog" arg
"${alt}" x
result=$("$runner")
