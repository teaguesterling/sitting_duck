#!/usr/bin/env bash
# Negative control for the conformance kit's CALL-NAMED check.
#
# bash `$(...)` command substitution is the known-legitimate unnamed-call shape
# (issue #91: "no single callee exists"). Every call-semantic node in this file
# therefore carries no callee name, so a language forced to DECLARE call naming
# while pointed here must FAIL the check. If it passes, the check cannot detect
# an unnamed call node and the run is void.
#
# Deliberately contains no plain `cmd arg` command either, so no named call can
# dilute the measurement.
a=$(date +%s)
b=$(hostname)
c=$(echo "$a$b")
