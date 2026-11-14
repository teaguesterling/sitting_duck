#!/bin/bash
# Is every committed parser in generated_parsers/ REPRODUCIBLE from its pin?
#
# WHY THIS EXISTS
# ---------------
# generated_parsers/ is the build's only parser source: CMakeLists sets
# PARSER_SOURCE_DIR to it whenever the canary tree-sitter-c/src/parser.c is
# present, which is every default build. The grammar submodules are not needed
# to build, and the `Runtime grammar registration tests` CI job proves that by
# checking out only duckdb, extension-ci-tools and third_party/tree-sitter.
#
# That decoupling has a cost nothing measured until 2026-11-13: a committed
# parser can stop corresponding to the pin it is supposed to come from, and
# nothing notices.
#
# It had already happened. The javascript pin moved on 2026-08-31 to a grammar
# with a first-class `using_declaration`; the typescript overlay stayed on a
# 2025-01-30 upstream that spelled the same construct its own way. From that day
# neither dialect of tree-sitter-typescript could be generated at all, and the
# committed typescript parser was a build against a javascript grammar the repo
# no longer pinned. `.ts` and `.js` silently disagreed about whether
# `using f = open()` declares anything. Eleven weeks later an unrelated PR (#329,
# the tsx language) tripped over it. See tracker/bugs/063.
#
# WHAT IT CHECKS, and what a pass means
# -------------------------------------
# For every language in generated_parsers/MANIFEST: apply this repo's patches,
# run `tree-sitter generate` in the grammar submodule, and compare the result
# byte-for-byte with the committed parser.c. Byte-identical is the only pass.
#
# A language may be exempt ONLY by declaring `regenerates: false` with a reason
# in spec/layout/layout.yaml's `exceptions`. An exempt language that starts
# regenerating is ALSO a finding -- the exception went stale -- so the contract
# is bidirectional, like spec/conformance/native-decl-baseline.tsv.
#
# COST, and why this is not a per-PR job
# --------------------------------------
# It regenerates 27 grammars. Measured 2026-11-14 on longbottom, by THIS script
# rather than by the hand survey that preceded it: 6m01s wall-clock with 4
# cores, 07:59:13 to 08:05:14. The header said "about 25 minutes" until that
# run; that figure came from the survey, which regenerated serially and counted
# differently, and no run of this script had ever completed when it was written.
# It still belongs on workflow_dispatch and a schedule rather than every push,
# because CI also has to build the CLI from source first. The cheap half of the same contract -- that every
# module's PARSERS path resolves inside generated_parsers/ and that every
# tree-sitter language has a MANIFEST line -- IS a per-PR check, in
# scripts/verify_layout_invariance.py.
#
# SIDE EFFECTS
# ------------
# Generation writes into the grammar submodules. Each is restored to its pin
# afterwards, so a clean tree stays clean; `--keep` skips the restore when you
# want to inspect the output.
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# The CLI, in order: an explicit TREE_SITTER_CLI, this tree's own build, then
# the MAIN WORKTREE's. That last one matters because this repo is used through
# git worktrees and `third_party/` is not checked out in them -- measured
# 2026-11-14, the first real run of this script exited 2 from a worktree while
# the binary sat in the main tree all along. CI needs none of this: its
# workflow builds the CLI from the pinned submodule before calling us.
if [ -n "${TREE_SITTER_CLI:-}" ]; then
    CLI="$TREE_SITTER_CLI"; CLI_FROM="TREE_SITTER_CLI"
elif [ -x "$ROOT/third_party/tree-sitter/target/release/tree-sitter" ]; then
    CLI="$ROOT/third_party/tree-sitter/target/release/tree-sitter"; CLI_FROM="this tree"
else
    _main_wt="$(git -C "$ROOT" worktree list --porcelain 2>/dev/null | awk '/^worktree /{print $2; exit}')"
    CLI="${_main_wt:-$ROOT}/third_party/tree-sitter/target/release/tree-sitter"; CLI_FROM="main worktree"
fi
KEEP=0
SELF_TEST=0
ONLY=""
for a in "$@"; do
  case "$a" in
    --keep) KEEP=1 ;;
    --self-test) SELF_TEST=1 ;;
    --language=*) ONLY="${a#--language=}" ;;
    -h|--help) sed -n '2,50p' "${BASH_SOURCE[0]}"; exit 0 ;;
    *) echo "unknown option: $a" >&2; exit 2 ;;
  esac
done

# patch names as scripts/apply_grammar_patches.sh matches them. They differ from
# the names scripts/generate_all_parsers.sh uses for the SAME patches; passing
# the wrong set is how the first draft of this check reported cpp as broken.
declare -A PATCH_NAME=(
  [cpp]=tree-sitter-cpp [typescript]=typescript [tsx]=tsx
  [rust]=tree-sitter-rust [php]=php
)

# `regenerates: false` languages, read from the spec rather than listed here.
exempt_reason() {
  python3 - "$ROOT" "$1" <<'PY'
import sys, pathlib
try:
    import yaml
except ImportError:
    sys.exit(0)
root, lang = sys.argv[1], sys.argv[2]
spec = yaml.safe_load((pathlib.Path(root) / "spec/layout/layout.yaml").read_text())
exc = (spec.get("exceptions") or {}).get(lang) or {}
if exc.get("regenerates") is False:
    print((exc.get("reason") or "no reason given").split("\n")[0][:70])
PY
}

compare() { cmp -s "$1" "$2" && echo IDENTICAL || echo DIFFERS; }

if [ "$SELF_TEST" -eq 1 ]; then
  # A comparison that cannot report a difference is not a comparison. Two files
  # that are certainly different, and one file against itself.
  a="$ROOT/generated_parsers/tree-sitter-c/src/parser.c"
  b="$ROOT/generated_parsers/tree-sitter-json/src/parser.c"
  [ "$(compare "$a" "$a")" = IDENTICAL ] || { echo "::error::self-test FAILED: a file does not match itself"; exit 1; }
  [ "$(compare "$a" "$b")" = DIFFERS ]   || { echo "::error::self-test FAILED: two different parsers compared equal"; exit 1; }
  if [ -z "$(exempt_reason yaml)" ]; then
    echo "::error::self-test FAILED: yaml is not declared \`regenerates: false\` in spec/layout/layout.yaml, so the exemption path is untested"; exit 1
  fi
  if [ -n "$(exempt_reason python)" ]; then
    echo "::error::self-test FAILED: python reads as exempt, so the exemption lookup matches too much"; exit 1
  fi
  echo "self-test OK: the comparison reports both IDENTICAL and DIFFERS, and the spec's exemption lookup finds yaml and only yaml"
  exit 0
fi

[ -x "$CLI" ] || { echo "::error::no tree-sitter CLI at $CLI (looked: TREE_SITTER_CLI, this tree, the main worktree). Build it: (cd third_party/tree-sitter && cargo build --release --bin tree-sitter)"; exit 2; }
cd "$ROOT" || exit 2
echo "cli:       $CLI ($CLI_FROM, $("$CLI" --version 2>/dev/null))"

printf '%-12s %-10s %-11s %s\n' LANGUAGE RESULT STATES NOTE
fail=0; ok=0; exempt=0; stale=0
while IFS=: read -r lang rel; do
  case "$lang" in ''|'#'*) continue ;; esac
  [ -n "$ONLY" ] && [ "$ONLY" != "$lang" ] && continue
  sub="grammars/${rel%%/*}"
  committed="generated_parsers/$rel/src/parser.c"
  regen="grammars/$rel/src/parser.c"
  reason="$(exempt_reason "$lang")"

  if [ ! -f "grammars/$rel/grammar.js" ]; then
    printf '%-12s %-10s %-11s %s\n' "$lang" "SKIP" "-" "no grammar.js in the submodule"
    continue
  fi

  st="$(mktemp -d)"
  [ -n "${PATCH_NAME[$lang]:-}" ] && bash scripts/apply_grammar_patches.sh "${PATCH_NAME[$lang]}" "$ROOT" "$ROOT/patches" "$st" >/dev/null 2>&1
  out="$( cd "grammars/$rel" && timeout 600 "$CLI" generate 2>&1 )"; rc=$?
  rm -rf "$st"
  states="$(grep -m1 '^#define STATE_COUNT' "$regen" 2>/dev/null | awk '{print $3}')"

  if [ "$rc" -ne 0 ]; then
    if [ -n "$reason" ]; then
      printf '%-12s %-10s %-11s %s\n' "$lang" "EXEMPT" "${states:--}" "declared: $reason"
      exempt=$((exempt+1))
    else
      printf '%-12s %-10s %-11s %s\n' "$lang" "FAIL" "${states:--}" "generate failed: $(printf '%s' "$out" | grep -m1 -A1 'Caused by:' | tail -1 | sed 's/^ *//' | cut -c1-60)"
      fail=$((fail+1))
    fi
  else
    verdict="$(compare "$regen" "$committed")"
    if [ "$verdict" = IDENTICAL ]; then
      if [ -n "$reason" ]; then
        printf '%-12s %-10s %-11s %s\n' "$lang" "STALE-EXC" "${states:--}" "regenerates now; remove its \`regenerates: false\` from the spec"
        stale=$((stale+1))
      else
        printf '%-12s %-10s %-11s %s\n' "$lang" "ok" "${states:--}" "byte-identical to the committed parser"
        ok=$((ok+1))
      fi
    else
      printf '%-12s %-10s %-11s %s\n' "$lang" "FAIL" "${states:--}" "regenerates, but NOT byte-identical to generated_parsers/"
      fail=$((fail+1))
    fi
  fi
  [ "$KEEP" -eq 1 ] || git -C "$sub" checkout -- . >/dev/null 2>&1
done < generated_parsers/MANIFEST

echo
echo "reproducible=$ok exempt=$exempt stale_exceptions=$stale failed=$fail"
if [ "$ok" -eq 0 ]; then
  echo "::error::0 parsers were compared. This check measured nothing, which is not a pass."
  exit 1
fi
if [ "$fail" -gt 0 ] || [ "$stale" -gt 0 ]; then
  echo "::error::generated_parsers/ no longer corresponds to the grammar pins. Either regenerate and commit the parser, or declare the language \`regenerates: false\` in spec/layout/layout.yaml with a reason."
  exit 1
fi
echo "OK: every committed parser is byte-identical to a regeneration from its pin, or declared exempt"
