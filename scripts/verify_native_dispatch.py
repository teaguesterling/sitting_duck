#!/usr/bin/env python3
"""Derivation gate for native extractors (#259, widened for #042 step 2).

#220 was three silent failures in three languages and NONE produced a compile
error, a link error or a warning: an extractor dispatched on node-type names the
grammar never emits, so every branch was dead and the declared strategy
delivered nothing. The conformance kit caught it; the compiler could not.

This asserts the thing a compiler cannot: every node-type string the extraction
layer dispatches on must exist in that grammar's symbol table.

The symbol table is the generated parser's own `ts_symbol_names[]`, which is
the authority on what a grammar can emit -- named rules and anonymous tokens
alike. A literal absent from it is a branch that can never be taken.

THREE CHECKS, because a node-type literal reaches the grammar three ways:

  A  dispatch   a comparison in a per-language extractor or adapter
  B  table      an entry in LANGUAGE_FUNCTION_CALL_TYPES, the shared table
  C  key        the language name a LanguageTag hands that table's lookup

Check C exists because `GetNodeTypes()` falls back to DEFAULT_FUNCTION_CALL_TYPES
on a lookup miss. A tag returning "c_sharp" against a map keyed "csharp" would
silently take the default -- the same populated-but-wrong class the other two
checks are for, one level up.

WHAT COUNTS AS A TYPE DISPATCH, and why it is no longer a name test. The first
version of this gate accepted `strcmp(<var with "type" in its name>, "lit")`.
That test is wrong in both directions, measured:

  false positive  cpp_native_extractors.hpp:255 binds `type_text` from
                  content.substr(), so it holds SOURCE TEXT. Its comparisons
                  ("static", "const", "virtual") are not node types.
  false positive  duckdb_adapter.cpp:748 binds `value_type` from
                  const_value.type() -- a DuckDB LogicalType.
  false negative  every `node_type == "lit"` comparison, because the name test
                  was only ever applied to strcmp. That was most of them.

So a variable qualifies by its BINDING, not its name: its nearest preceding
assignment in the same file must resolve, through `string(...)` wrappers, to
`ts_node_type(...)`.

A string PARAMETER has no such assignment, so a parameter declaration ends any
binding of that name. Stopping there loses real coverage: helpers like
`IsTypeNode(const char *child_type)` hold 17 live dispatches whose value comes
from the caller, and a purely lexical rule dropped 13 literals the first
version of this gate did judge. So a parameter is re-bound when, and only when,
EVERY call site of its function passes a node type. One call site passing text
leaves it unresolved, because a gate that judges a parameter on a majority
would report a live branch as dead.

Every unjudged site is counted and printed BY OPERAND, so the denominator is
never silently zero and a coverage hole is visible as a name: 17 sites on
`child_type` was the hole; 15 on `name` is correct.
"""
import collections
import re
import sys
import tempfile
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parent.parent
SPEC = ROOT / "spec" / "abi" / "module-abi.yaml"
SHARED_TABLE = ROOT / "src" / "include" / "function_call_extractor.hpp"

# tree-sitter's generated grammar directory does not always match our language
# name.
GRAMMAR_DIR_FOR = {"csharp": "c-sharp"}

# Adapters with no tree-sitter grammar behind them, excluded by name rather
# than by a missing-grammar skip, so that a real grammar going missing is still
# reported. duckdb wraps DuckDB's own parser; dynamic_language is the
# out-of-tree JSON path; the _backup file is not compiled.
NOT_TREE_SITTER = {"duckdb", "dynamic_language", "duckdb_adapter_backup.cpp"}

SYMBOL = re.compile(r'\]\s*=\s*"((?:[^"\\]|\\.)*)"\s*,')

# A real assignment: '=' neither part of a comparison nor of an op-assign.
ASSIGN = re.compile(r'\b([A-Za-z_][A-Za-z0-9_]*)\s*(?<![=!<>+\-*/%&|^])=(?!=)\s*(.*)')

# A parameter declaration ends a binding: `const string &node_type,`.
# Used while resolving call sites, where a parameter must not inherit the
# binding an earlier function in the same file left behind.
PARAM = re.compile(
    r'(?:const\s+)?(?:std::)?(?:string|string_view|char)\s*[&*]\s*'
    r'([A-Za-z_][A-Za-z0-9_]*)\s*[,)]'
)

STRCMP = re.compile(
    r'strcmp\s*\(\s*(ts_node_type\s*\([^()]*\)|[A-Za-z_][A-Za-z0-9_]*)\s*,\s*'
    r'"((?:[^"\\]|\\.)*)"\s*\)'
)
EQCMP = re.compile(
    r'([A-Za-z_][A-Za-z0-9_:]*(?:\s*\((?:[^()]|\([^()]*\))*\))?)\s*(?:==|!=)\s*'
    r'"((?:[^"\\]|\\.)*)"'
)

C_ESCAPES = {'"': '"', "\\": "\\", "?": "?", "n": "\n", "0": "\0", "t": "\t",
             "r": "\r", "'": "'", "a": "\a", "b": "\b", "f": "\f", "v": "\v"}


def decode_c_string(raw: str) -> str:
    """Decode the C escapes a generated parser.c actually carries.

    Sequential str.replace() is wrong here: replacing \\\\ after \\" lets the
    two interfere. `unicode_escape` is also wrong -- it mangles \\? and raises
    on some sequences, so the gate would fail for the wrong reason.

    Measured across every generated_parsers/**/parser.c symbol table: \\" (67),
    \\? (63), \\\\ (8), \\n (3), \\0 (1). The first version of this gate handled
    only \\" and \\\\, so all 63 \\? names decoded wrongly -- which made
    tree-sitter-dart's "?." read as absent when the grammar emits it. A gate
    whose input is wrong reports confident nonsense.
    """
    out = []
    i = 0
    while i < len(raw):
        c = raw[i]
        if c == "\\" and i + 1 < len(raw):
            nxt = raw[i + 1]
            out.append(C_ESCAPES.get(nxt, nxt))
            i += 2
        else:
            out.append(c)
            i += 1
    return "".join(out)


def strip_comments(lines):
    """Yield each line with its comments removed, quotes respected.

    A naive `//` strip would cut `strcmp(t, "http://x")` in half, and a naive
    comment skip would miss a dispatch that shares a line with a trailing note.
    """
    in_block = False
    for line in lines:
        out = []
        i = 0
        in_str = False
        while i < len(line):
            c = line[i]
            if in_block:
                if line.startswith("*/", i):
                    in_block = False
                    i += 2
                    continue
                i += 1
                continue
            if in_str:
                out.append(c)
                if c == "\\" and i + 1 < len(line):
                    out.append(line[i + 1])
                    i += 2
                    continue
                if c == '"':
                    in_str = False
                i += 1
                continue
            if c == '"':
                in_str = True
                out.append(c)
                i += 1
                continue
            if line.startswith("//", i):
                break
            if line.startswith("/*", i):
                in_block = True
                i += 2
                continue
            out.append(c)
            i += 1
        yield "".join(out)


def symbol_table(language: str):
    d = GRAMMAR_DIR_FOR.get(language, language)
    base = ROOT / "generated_parsers" / f"tree-sitter-{d}"
    # Some grammars nest the parser one level down (typescript/, php/), which is
    # why this searches rather than assuming src/parser.c. Skipping them silently
    # is exactly the partial coverage this gate exists to prevent elsewhere.
    parser = base / "src" / "parser.c"
    if not parser.exists():
        candidates = sorted(base.glob(f"{d}/src/parser.c")) or sorted(base.glob("*/src/parser.c"))
        parser = candidates[0] if candidates else None
    if parser is None or not parser.exists():
        return None
    text = parser.read_text(encoding="utf-8", errors="replace")
    start = text.find("ts_symbol_names[] = {")
    if start < 0:
        return None
    body = text[start:text.find("};", start)]
    return {decode_c_string(m.group(1)) for m in SYMBOL.finditer(body)}


# --------------------------------------------------------------------------
# Check A: dispatch comparisons, judged by binding
# --------------------------------------------------------------------------

def classify(expr: str, bound: dict):
    """True if `expr` evaluates to a node type, False if not, None if unknown."""
    expr = expr.strip()
    m = re.fullmatch(r'(?:std::)?string\s*\(\s*(.*)\s*\)', expr)
    if m:
        return classify(m.group(1), bound)
    if re.match(r'ts_node_type\s*\(', expr):
        return True
    if re.fullmatch(r'[A-Za-z_][A-Za-z0-9_]*', expr):
        return bound.get(expr)
    return False


# A function definition whose parameter list may hold a string-ish parameter.
# These signatures are one line or two in this codebase, so the scan joins
# continuation lines until the parameter list closes.
FUNCDEF = re.compile(r'\b([A-Za-z_][A-Za-z0-9_]*)\s*\(')
NOT_A_CALL = frozenset({"if", "for", "while", "switch", "return", "sizeof",
                        "strcmp", "strncmp", "string", "catch"})
STRING_PARAM = re.compile(
    r'(?:const\s+)?(?:std::)?(?:string|string_view|char)\s*[&*]?\s*'
    r'([A-Za-z_][A-Za-z0-9_]*)\s*$'
)


def split_args(text: str):
    """Split a call's argument list on top-level commas."""
    out, depth, cur = [], 0, []
    for c in text:
        if c in "([{":
            depth += 1
        elif c in ")]}":
            depth -= 1
        if c == "," and depth == 0:
            out.append("".join(cur))
            cur = []
            continue
        cur.append(c)
    if "".join(cur).strip():
        out.append("".join(cur))
    return [a.strip() for a in out]


def paren_span(lines, lineno, col):
    """Return the text inside the parentheses that open at (lineno, col)."""
    depth, buf = 0, []
    for i in range(lineno, min(lineno + 6, len(lines))):
        line = lines[i]
        start = col if i == lineno else 0
        for j in range(start, len(line)):
            c = line[j]
            if c == "(":
                depth += 1
                if depth == 1:
                    continue
            elif c == ")":
                depth -= 1
                if depth == 0:
                    return "".join(buf)
            if depth >= 1:
                buf.append(c)
    return None


def string_parameters(files):
    """Map function name -> {parameter index: parameter name} for string params.

    A parameter's value is invisible where it is used, so these are the sites a
    purely lexical binding test must give up on -- 45 of them, measured, and 17
    carry real node-type dispatches inside helpers like
    cpp_native_extractors.hpp:269 `IsTypeNode(const char *child_type)`.
    """
    sigs = {}
    for path in files:
        lines = list(strip_comments(path.read_text(encoding="utf-8", errors="replace").splitlines()))
        for i, line in enumerate(lines):
            for m in FUNCDEF.finditer(line):
                name = m.group(1)
                if name in NOT_A_CALL:
                    continue
                args = paren_span(lines, i, m.start(1) + len(name))
                if args is None:
                    continue
                found = {}
                for idx, arg in enumerate(split_args(args)):
                    pm = STRING_PARAM.search(arg)
                    if pm:
                        found[idx] = pm.group(1)
                if found:
                    sigs.setdefault(name, {}).update(found)
    return sigs


def resolve_parameters(files, sigs):
    """Bind a string parameter when EVERY call site passes a node type.

    One call site passing text is enough to leave the parameter unresolved: a
    gate that judges a parameter on a majority would report a live branch as
    dead. A parameter left unresolved is not dropped quietly: its comparison
    sites appear by operand name in the denominator the gate prints.

    None means the function has no call site at all, which is itself worth
    knowing -- a helper nobody calls.
    """
    seen = {(name, pname): set()
            for name, params in sigs.items() for pname in params.values()}
    for path in files:
        lines = list(strip_comments(path.read_text(encoding="utf-8", errors="replace").splitlines()))
        bound = {}
        for i, line in enumerate(lines):
            for n in PARAM.findall(line):
                bound[n] = None
            for m in ASSIGN.finditer(line):
                bound[m.group(1)] = classify(m.group(2).rstrip(";"), bound)
            for m in FUNCDEF.finditer(line):
                name = m.group(1)
                if name not in sigs:
                    continue
                args = paren_span(lines, i, m.start(1) + len(name))
                if args is None:
                    continue
                parts = split_args(args)
                for idx, pname in sigs[name].items():
                    if idx >= len(parts):
                        continue
                    arg = parts[idx]
                    # The definition itself looks like a call; its argument is
                    # the declaration, which classifies as not-a-node-type and
                    # would veto every real call site.
                    if STRING_PARAM.search(arg):
                        continue
                    seen[(name, pname)].add(bool(classify(arg, bound)))
    return {key: (states == {True} if states else None)
            for key, states in seen.items()}


def scan_dispatch(path: Path, param_bound=None):
    """Return (type_literals, unbound) for one source file.

    type_literals is a list of (literal, lineno) whose left operand holds a
    node type. unbound counts, by operand, the comparisons against a string
    literal whose left operand does not -- the denominator this gate cannot
    see. Naming the operands is what makes the denominator reviewable: 17 sites
    on `child_type` is a coverage hole worth closing, 15 on `name` is correct.
    """
    param_bound = param_bound or {}
    bound = {}
    literals = []
    unbound = collections.Counter()
    raw = path.read_text(encoding="utf-8", errors="replace").splitlines()
    lines = list(strip_comments(raw))
    for lineno, line in enumerate(lines, 1):
        # A parameter declaration ends any lexical binding of that name, rather
        # than inheriting one from an earlier function. It is re-bound only if
        # every call site of its function passed a node type.
        for m in FUNCDEF.finditer(line):
            name = m.group(1)
            if name in NOT_A_CALL:
                continue
            args = paren_span(lines, lineno - 1, m.start(1) + len(name))
            if args is None:
                continue
            for arg in split_args(args):
                pm = STRING_PARAM.search(arg)
                if pm:
                    bound[pm.group(1)] = param_bound.get((name, pm.group(1)))
        for m in ASSIGN.finditer(line):
            bound[m.group(1)] = classify(m.group(2).rstrip(";"), bound)
        for m in STRCMP.finditer(line):
            if classify(m.group(1), bound):
                literals.append((decode_c_string(m.group(2)), lineno))
            else:
                unbound[m.group(1).strip()] += 1
        for m in EQCMP.finditer(line):
            if "strcmp" in m.group(1):
                continue
            if classify(m.group(1), bound):
                literals.append((decode_c_string(m.group(2)), lineno))
            else:
                unbound[m.group(1).strip()] += 1
    return literals, unbound


# --------------------------------------------------------------------------
# Check B/C: the shared LANGUAGE_FUNCTION_CALL_TYPES table
# --------------------------------------------------------------------------

def shared_table_entries(lines):
    """Yield (language, literal, lineno) for each literal in each map entry.

    DEFAULT_FUNCTION_CALL_TYPES is deliberately NOT attributed to a language.
    Measured: 15 LanguageTags, 15 map keys, all matching, and 15 instantiations
    of UnifiedFunctionCallExtractor. lua includes this header but instantiates
    nothing, so the default has no consumer. Blaming its literals on lua would
    have invented a bug.
    """
    opened = next((i for i, line in enumerate(lines)
                   if "LANGUAGE_FUNCTION_CALL_TYPES = {" in line), None)
    if opened is None:
        return
    # Brace-count to the map's close. The close is written `    }}};`, indented,
    # so a startswith("};") test walks off the end of the map and attributes the
    # whole rest of the file to its last language -- which it did, and reported
    # seven of this header's own generic helpers as dead dart node types.
    depth = 0
    end = len(lines)
    for i in range(opened, len(lines)):
        depth += lines[i].count("{") - lines[i].count("}")
        if depth <= 0 and i > opened:
            end = i
            break
    starts = [(i, m.group(1)) for i, line in enumerate(lines[:end])
              if (m := re.match(r'^    \{"([a-z_]+)",\s*$', line)) and i > opened]
    if not starts:
        return
    bounds = [s[0] for s in starts] + [end]
    for (first, language), stop in zip(starts, bounds[1:]):
        for i in range(first, stop):
            for lit in re.findall(r'"((?:[^"\\]|\\.)*)"', lines[i]):
                if i == first and lit == language:
                    continue
                yield language, decode_c_string(lit), i + 1


def shared_table_keys(lines):
    return {m.group(1) for line in lines
            if (m := re.match(r'^    \{"([a-z_]+)",\s*$', line))}


def tag_language_names(lines):
    """Map LanguageTag struct name -> the language name it returns."""
    tags = {}
    current = None
    for line in lines:
        m = re.match(r'\s*struct\s+([A-Za-z0-9_]+LanguageTag)\s*\{', line)
        if m:
            current = m.group(1)
            continue
        if current:
            m = re.search(r'return\s+"([^"]*)"\s*;', line)
            if m:
                tags[current] = m.group(1)
                current = None
    return tags


def key_findings(tags, keys, live):
    """Instantiated tags whose language name is not a key of the table.

    Factored out so the self-test plants its fault in the SAME code main()
    runs. A self-test that recomputes the decision inline proves only that the
    test agrees with itself.
    """
    out = []
    for tag in sorted(live):
        name = tags.get(tag)
        if name is None:
            out.append((tag, None))
        elif name not in keys:
            out.append((tag, name))
    return out


def instantiated_tags():
    found = set()
    for path in sorted((ROOT / "src").rglob("*.hpp")) + sorted((ROOT / "src").rglob("*.cpp")):
        for m in re.finditer(r'UnifiedFunctionCallExtractor\s*<\s*([A-Za-z0-9_]+)\s*>',
                             path.read_text(encoding="utf-8", errors="replace")):
            found.add(m.group(1))
    return found


# --------------------------------------------------------------------------

def self_test() -> int:
    """Planted-fault control: prove each check can fail, not just pass.

    A gate that cannot be made to fail is indistinguishable from one that is
    not looking. Every check gets a plant, and every plant is one of the ways
    this gate has actually been wrong:

      check A  a dead `==` dispatch, the idiom the first version never read
               a live one beside it, so the plant is not "everything fails"
               the real text-bound negative from cpp_native_extractors.hpp:279
               a parameter-bound site, which must be unjudged, not judged
      check B  a table entry whose literals must be read against ITS language,
               and whose closing `    }}};` must end the map
      check C  a tag whose language name is not a key of the map
      input    the C-escape decoder, which read 63 symbol names wrongly
    """
    symbols = symbol_table("kotlin")
    if not symbols:
        print("::error::self-test could not load the kotlin symbol table")
        return 1

    good = "simple_identifier"          # really in tree-sitter-kotlin
    bad = "function_value_parameter"    # the singular form -- the #220 bug
    if good not in symbols:
        print(f"::error::self-test fixture is stale: {good!r} is no longer in the grammar")
        return 1
    if bad in symbols:
        print(f"::error::self-test fixture is stale: {bad!r} IS in the grammar now")
        return 1

    handle = tempfile.NamedTemporaryFile("w", suffix=".hpp", delete=False,
                                        encoding="utf-8")
    handle.close()
    fixture = Path(handle.name)
    fixture.write_text(
        "void live(TSNode n) {\n"
        "  string node_type = ts_node_type(n);\n"
        f'  if (node_type == "{good}" || node_type == "{bad}") {{}}\n'
        "}\n"
        "bool IsModifier(TSNode n, const string &content) {\n"
        "  string type_text = content.substr(0, 3);\n"
        '  return type_text == "static" || type_text == "nonsuchnodetype";\n'
        "}\n"
        "void param_bound(const string &node_type) {\n"
        '  if (node_type == "alsonotanodetype") {}\n'
        "}\n",
        encoding="utf-8",
    )
    literals, unbound = scan_dispatch(fixture)
    fixture.unlink()

    got = sorted(lit for lit, _ in literals)
    if got != sorted([good, bad]):
        print(f"::error::self-test FAILED: expected the two ts_node_type-bound "
              f"literals {sorted([good, bad])}, got {got}")
        return 1
    dead = [lit for lit, _ in literals if lit not in symbols]
    if dead != [bad]:
        print(f"::error::self-test FAILED: expected exactly ['{bad}'] dead, got {dead}")
        return 1
    if dict(unbound) != {"type_text": 2, "node_type": 1}:
        print(f"::error::self-test FAILED: expected 2 text-bound type_text "
              f"sites and 1 parameter-bound node_type site left unjudged, "
              f"got {dict(unbound)}")
        return 1

    # Check B: the entry's literals belong to ITS language, and the indented
    # `    }}};` must end the map. The first version ran past it.
    table_fixture = [
        'static const std::unordered_map<string, FunctionCallNodeTypes> '
        'LANGUAGE_FUNCTION_CALL_TYPES = {',
        '    {"alpha",',
        '     {',
        '         "alpha_call",',
        '     }},',
        '    {"omega",',
        '     {',
        '         "omega_call",',
        '     }}};',
        '',
        'static void NotPartOfTheMap() {',
        '    const char *x = "should_not_be_attributed_to_omega";',
        '}',
    ]
    entries = [(lang, lit) for lang, lit, _ in shared_table_entries(table_fixture)]
    if entries != [("alpha", "alpha_call"), ("omega", "omega_call")]:
        print(f"::error::self-test FAILED: check B read {entries}, expected one "
              f"literal per language and nothing past the map")
        return 1
    if shared_table_keys(table_fixture) != {"alpha", "omega"}:
        print("::error::self-test FAILED: check B did not read the map's keys")
        return 1

    # Check C: a tag whose name misses the map takes DEFAULT in silence.
    tag_fixture = [
        "struct AlphaLanguageTag {",
        '    static string GetLanguageName() { return "alpha"; }',
        "};",
        "struct OmegaLanguageTag {",
        '    static string GetLanguageName() { return "om_ega"; }',
        "};",
    ]
    tags = tag_language_names(tag_fixture)
    if tags != {"AlphaLanguageTag": "alpha", "OmegaLanguageTag": "om_ega"}:
        print(f"::error::self-test FAILED: check C read tags as {tags}")
        return 1
    flagged = key_findings(tags, shared_table_keys(table_fixture), set(tags))
    if flagged != [("OmegaLanguageTag", "om_ega")]:
        print(f"::error::self-test FAILED: check C should flag exactly the "
              f"omega tag, flagged {flagged}")
        return 1

    # The decoder is the gate's input. A wrong decoder reports confident
    # nonsense. Three assertions, each catching a different wrong decoder:
    #   \?.  the old gate left the backslash in, so tree-sitter-dart's "?."
    #        read as absent. An identity escape must drop its backslash.
    #   \n   an escape that maps to a DIFFERENT character. The fallback cannot
    #        serve this one, so it is what makes the table load-bearing --
    #        checked because a mutant that deleted the "?" entry still passed.
    #   \\?  a backslash followed by ?, which a decoder that scans right-to-
    #        left or replaces sequentially gets wrong.
    decoder = {r"\?.": "?.", r"\n": "\n", r"\\?": "\\?"}
    for raw, want in decoder.items():
        if decode_c_string(raw) != want:
            print(f"::error::self-test FAILED: decode_c_string({raw!r}) gave "
                  f"{decode_c_string(raw)!r}, want {want!r}")
            return 1

    print("self-test OK: all three checks fail on a planted fault — a dead == "
          "dispatch, a map that ends where it ends, a tag that misses its key — "
          "and text- and parameter-bound sites stay unjudged")
    return 0


def main() -> int:
    if "--self-test" in sys.argv:
        return self_test()

    spec = yaml.safe_load(SPEC.read_text(encoding="utf-8"))
    baseline = {}
    for entry in (spec.get("native_extraction") or {}).get("dead_dispatch") or []:
        baseline[(entry["check"], entry["language"], entry["literal"])] = entry

    headers = {p.name.replace("_native_extractors.hpp", ""): p
               for p in (ROOT / "src" / "include").glob("*_native_extractors.hpp")}
    # One directory per language since 054 M2 stage 1; this was
    # src/language_adapters/*_adapter.cpp before that move. The path is built
    # from components, so the string "src/language_adapters" never appeared in
    # this file -- which is why a grep for it missed this gate and the move
    # broke it. It failed loudly (exit 1, "looking in the wrong place"), which
    # is the non-vacuity guard below doing its job.
    adapters = {p.name.replace("_adapter.cpp", ""): p
                for p in (ROOT / "languages").glob("*/*_adapter.cpp")
                if p.name not in NOT_TREE_SITTER
                and p.name.replace("_adapter.cpp", "") not in NOT_TREE_SITTER}
    if not headers:
        print("::error::no *_native_extractors.hpp found; this check is looking in the wrong place")
        return 1
    if not adapters:
        print("::error::no *_adapter.cpp found; this check is looking in the wrong place")
        return 1

    languages = sorted(set(headers) | set(adapters))
    # Pass one: which string parameters every call site feeds a node type.
    # This must run over every file before any file is judged, because a helper
    # is defined in one header and called from another.
    sources = sorted(set(headers.values()) | set(adapters.values()) | {SHARED_TABLE})
    sigs = string_parameters(sources)
    param_bound = resolve_parameters(sources, sigs)
    resolved = sum(1 for v in param_bound.values() if v)

    findings = []
    no_grammar = []
    judged = {}
    unbound_by_operand = collections.Counter()
    unjudged = []

    for language in languages:
        symbols = symbol_table(language)
        if symbols is None:
            no_grammar.append(language)
            continue
        if not symbols:
            print(f"::error::{language}: parsed an EMPTY symbol table; "
                  f"the parse is wrong, not the grammar")
            return 1
        count = 0
        seen = {}
        for path in [p for p in (headers.get(language), adapters.get(language)) if p]:
            literals, unbound = scan_dispatch(path, param_bound)
            unbound_by_operand.update(unbound)
            count += len(literals)
            for lit, lineno in literals:
                if lit not in symbols:
                    seen.setdefault(lit, (path, lineno))
        judged[language] = count
        if count == 0:
            unjudged.append(language)
        for lit, (path, lineno) in sorted(seen.items()):
            findings.append(("dispatch", language, lit,
                             path.relative_to(ROOT).as_posix(), lineno))

    # ---- checks B and C ----
    table_lines = SHARED_TABLE.read_text(encoding="utf-8", errors="replace").splitlines()
    table_literals = 0
    table_seen = {}
    for language, lit, lineno in shared_table_entries(table_lines):
        table_literals += 1
        symbols = symbol_table(language)
        if symbols is None:
            continue
        if lit not in symbols:
            table_seen.setdefault((language, lit), lineno)
    for (language, lit), lineno in sorted(table_seen.items()):
        findings.append(("table", language, lit,
                         SHARED_TABLE.relative_to(ROOT).as_posix(), lineno))

    keys = shared_table_keys(table_lines)
    tags = tag_language_names(table_lines)
    live = instantiated_tags()
    for tag, name in key_findings(tags, keys, live):
        if name is None:
            print(f"::error::{tag} is instantiated but this gate cannot read the "
                  f"language name it returns")
            return 1
        findings.append(("key", name, tag,
                         SHARED_TABLE.relative_to(ROOT).as_posix(), 0))

    # ---- report ----
    print(f"checked {sum(judged.values())} ts_node_type-bound dispatch literals "
          f"in {len(judged)} languages, {table_literals} literals in "
          f"{len(keys)} shared-table entries, {len(live)} instantiated tags")
    print(f"string parameters: {resolved} of {len(param_bound)} bound by every "
          f"call site. The rest are never compared to a node-type literal; any "
          f"that are appear by name below.")
    print(f"not judged: {sum(unbound_by_operand.values())} comparison sites "
          f"whose left operand is not a node type; by operand: "
          + ", ".join(f"{op}({n})" for op, n in unbound_by_operand.most_common(8)))
    if no_grammar:
        print(f"no grammar found, skipped: {', '.join(no_grammar)}")
    if unjudged:
        print(f"zero dispatch literals, nothing to judge: {', '.join(unjudged)}")
    missing_header = sorted(set(adapters) - set(headers))
    if missing_header:
        print(f"no native extractor header: {', '.join(missing_header)}")
    excluded = sorted(n for n in NOT_TREE_SITTER if not n.endswith(".cpp"))
    print(f"excluded, no tree-sitter grammar behind the adapter: "
          f"{', '.join(excluded)}")

    # NON-VACUITY: a regex that matched nothing would pass silently, which is
    # exactly the failure mode this gate exists to catch elsewhere.
    if sum(judged.values()) == 0:
        print("::error::matched 0 dispatch literals; the pattern is wrong, not the code")
        return 1
    if table_literals == 0:
        print("::error::read 0 literals from LANGUAGE_FUNCTION_CALL_TYPES; "
              "the table moved or its shape changed")
        return 1

    # ---- baseline, both directions ----
    found_keys = {(check, lang, lit) for check, lang, lit, _, _ in findings}
    new = [f for f in findings if (f[0], f[1], f[2]) not in baseline]
    stale = sorted(set(baseline) - found_keys)

    if "--emit-baseline" in sys.argv:
        # The baseline is generated, never hand-typed. 70 entries typed by hand
        # would carry typos that read as "fixed" and silently stop being gated.
        print("  dead_dispatch:")
        for check, language, lit, where, lineno in findings:
            at = f"{Path(where).name}:{lineno}" if lineno else Path(where).name
            print(f'    - {{check: {check}, language: {language}, '
                  f'literal: "{lit}"}}   # {at}')
        return 0

    if new:
        print()
        for check, language, lit, where, lineno in new:
            if check == "key":
                print(f"::error file={where}::{lit} returns language name "
                      f"\"{language}\", which is not a key of "
                      f"LANGUAGE_FUNCTION_CALL_TYPES — it silently takes "
                      f"DEFAULT_FUNCTION_CALL_TYPES")
            else:
                print(f"::error file={where},line={lineno}::"
                      f"{language} {check} uses \"{lit}\", which is not in the "
                      f"tree-sitter-{GRAMMAR_DIR_FOR.get(language, language)} "
                      f"symbol table — this branch can never be taken")
        print(f"\n{len(new)} dead node-type literal(s) not in the "
              f"spec baseline (native_extraction.dead_dispatch)")

    if stale:
        print()
        for check, language, lit in stale:
            print(f"::error file=spec/abi/module-abi.yaml::dead_dispatch baselines "
                  f"{language} {check} \"{lit}\", but this run does not reproduce "
                  f"it — the literal was fixed, deleted, or the grammar now emits "
                  f"it. Remove the baseline entry.")
        print(f"\n{len(stale)} stale baseline entr(ies)")

    if new or stale:
        return 1

    if baseline:
        print(f"OK: {len(baseline)} known-dead literal(s) still exactly as "
              f"spec/abi/module-abi.yaml records them, and no new ones")
    else:
        print("OK: every node-type literal exists in its grammar")
    return 0


if __name__ == "__main__":
    sys.exit(main())
