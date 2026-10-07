#!/usr/bin/env bash
# leading comment
set -eu
name="world"
printf '%s\n' "hello ${name}"
count=42
if [ "$count" -gt 7 ]; then
  echo 'single quoted'
fi
