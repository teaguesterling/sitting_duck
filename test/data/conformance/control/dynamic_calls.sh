#!/usr/bin/env bash
# Negative control for the #89 "can't answer -> raise, never silent empty" guard
# on callee-name binding (test/sql/bugs/issue_88_89_callee_name_and_guards.test
# and test/sql/conformance/no_silent_empty.test).
#
# WHAT THIS FILE MUST BE. The guard raises only when NO call node in the source
# carries a name, so every call node here must be unnamed. One named call node
# satisfies the guard's EXISTS, the guard stops raising, and three `statement
# error` assertions assert an error that can no longer happen.
#
# IT WAS VOIDED TWICE IN ONE DAY, BOTH TIMES BY A NAMING RULING. The shapes it
# relied on kept becoming named:
#
#   #140a    named `date` and `echo` in test/data/call_extraction/calls.sh, so
#            the guard's original source became answerable. The shape moved here.
#   2026-11-18a  a bash command named by a VARIABLE is named as written, so
#                `$cmd --flag` reports `$cmd`. Rewritten to use strings.
#   2026-11-18b  "that command's name is the whole string: $prefix/bin/$tool".
#                `"$prog" arg` reports `$prog`. The strings went too.
#
# SO THIS VERSION HOLDS NO `command` NODE AT ALL, which is why it should be the
# last rewrite. `$(<file)` is a command substitution whose body is a REDIRECT:
# measured, it parses as one `command_substitution` (COMPUTATION_CALL) wrapping
# a `file_redirect`, with no `command` inside it and nothing anywhere that a
# name could be read from. #91 holds that a substitution has no single callee,
# and here there is not even an inner command to name instead. A naming rule
# cannot reach this file, because naming needs a name position and this file
# has none.
#
# DO NOT ADD any command at all. A single `cmd arg`, `$cmd`, `"$cmd"`, `'cmd'`,
# or `$(cmd)` -- whose INNER command is named -- voids the control.
#
# Measured after each change: `SELECT count(*) FILTER (WHERE name <> '')` over
# this file's call rows must be 0, and the row count must be > 0. A file with no
# call nodes would make the guards raise for a different reason.
first=$(<one.txt)
second=$(<two.txt)
third=`<three.txt`
