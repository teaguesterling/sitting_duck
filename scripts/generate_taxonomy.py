#!/usr/bin/env python3
"""Generate the C++ taxonomy tables from spec/taxonomy/taxonomy.yaml.

The universal taxonomy — the 8-bit semantic_type encoding, the node flag byte,
and the name/native extraction strategy enums — used to be hand-maintained in
four parallel C++ files. They drifted, and the drift caused real bugs (the
flag-name/enum sync issue behind #80). This script makes the spec the single
definition and regenerates the C++ from it.

Each generated block lives between BEGIN/END markers inside its existing file,
so nothing moves and non-taxonomy code in those files is untouched. The
committed C++ and the spec are kept in lockstep by the "taxonomy tables in
sync" CI check, exactly like scripts/embed_sql_macros.py and
src/include/embedded_sql_macros.hpp.

Usage:
    python3 scripts/generate_taxonomy.py              # rewrite the marked regions
    python3 scripts/generate_taxonomy.py --check      # fail if any region is stale
    python3 scripts/generate_taxonomy.py --emit ID    # print one region to stdout

Determinism: output depends only on the spec's contents and declaration order.
Nothing is sorted implicitly, nothing is keyed on a dict's iteration order, and
running the script twice is a no-op.
"""

import argparse
import sys
from pathlib import Path

try:
    import yaml
except ImportError:  # pragma: no cover - environment problem, not a logic path
    sys.exit(
        "generate_taxonomy.py needs PyYAML (python3 -m pip install pyyaml)"
    )

PROJECT_ROOT = Path(__file__).resolve().parent.parent
SPEC_PATH = PROJECT_ROOT / "spec" / "taxonomy" / "taxonomy.yaml"

# Marker lines delimiting a generated block inside an existing source file.
BEGIN_FMT = "// <<< BEGIN GENERATED TAXONOMY: {region} >>>"
END_FMT = "// <<< END GENERATED TAXONOMY: {region} >>>"
MARKER_NOTE = (
    "// Generated from spec/taxonomy/taxonomy.yaml by scripts/generate_taxonomy.py."
)
MARKER_NOTE2 = "// Edit the spec, not this block."


# ---------------------------------------------------------------------------
# Spec model
# ---------------------------------------------------------------------------


class Spec:
    """The taxonomy spec, with the derived codes every renderer needs.

    Lists are kept in spec order, which is the C++ declaration order. The one
    place the hand-written C++ uses a different order (GetKindName and
    GetKindCode list kinds by numeric code rather than by declaration) is
    served by kinds_by_code(); see rendering.super_kind_declaration_order in
    the spec for why both orders exist.
    """

    def __init__(self, data):
        self.version = data["taxonomy_spec_version"]
        self._declaration_order = data["rendering"]["super_kind_declaration_order"]
        self.super_kinds = data["super_kinds"]
        self.kinds = data["kinds"]
        self.semantic_types = data["semantic_types"]
        self.flag_byte = data["flag_byte"]
        self.strategies = data["strategies"]

        self._super_kind_code = {sk["name"]: sk["code"] for sk in self.super_kinds}
        self._kind_code = {}
        for kind in self.kinds:
            super_kind = kind["super_kind"]
            if super_kind not in self._super_kind_code:
                raise ValueError(
                    "kind %s names unknown super_kind %s" % (kind["name"], super_kind)
                )
            self._kind_code[kind["name"]] = (
                self._super_kind_code[super_kind] | kind["offset"]
            )

        self._type_code = {}
        for st in self.semantic_types:
            kind = st["kind"]
            if kind not in self._kind_code:
                raise ValueError(
                    "semantic_type %s names unknown kind %s" % (st["name"], kind)
                )
            self._type_code[st["name"]] = self._kind_code[kind] | st["offset"]

        self._validate()

    def _validate(self):
        """Catch spec mistakes here rather than letting them reach the C++."""
        if len(self.super_kinds) != 4:
            raise ValueError("the encoding has exactly 4 super kinds")
        if len(self.kinds) != 16:
            raise ValueError("the encoding has exactly 16 kinds (4 per super kind)")
        if len(self.semantic_types) != 64:
            raise ValueError("the encoding has exactly 64 semantic types (4 per kind)")

        seen = {}
        for name, code in self._type_code.items():
            if code in seen:
                raise ValueError(
                    "semantic types %s and %s both encode to 0x%02X"
                    % (seen[code], name, code)
                )
            seen[code] = name

        names = [sk["name"] for sk in self.super_kinds]
        for name in self.declaration_order:
            if name not in names:
                raise ValueError(
                    "rendering.super_kind_declaration_order names unknown super kind %s"
                    % name
                )
        if sorted(self.declaration_order) != sorted(names):
            raise ValueError(
                "rendering.super_kind_declaration_order must list every super kind once"
            )

    # -- codes --------------------------------------------------------------

    def super_kind_code(self, name):
        return self._super_kind_code[name]

    def kind_code(self, name):
        return self._kind_code[name]

    def type_code(self, name):
        return self._type_code[name]

    # -- orderings ----------------------------------------------------------

    @property
    def declaration_order(self):
        return self._declaration_order

    def kinds_by_code(self):
        return sorted(self.kinds, key=lambda k: self._kind_code[k["name"]])

    def kind_groups_declared(self):
        """[(kind, [semantic_type, ...]), ...] in C++ declaration order."""
        by_kind = {}
        for st in self.semantic_types:
            by_kind.setdefault(st["kind"], []).append(st)
        return [(kind, by_kind[kind["name"]]) for kind in self.kinds]

    def kind_groups_by_code(self):
        """[(super_kind_name, [kind, ...]), ...] grouped in numeric code order."""
        groups = []
        for kind in self.kinds_by_code():
            if not groups or groups[-1][0] != kind["super_kind"]:
                groups.append((kind["super_kind"], []))
            groups[-1][1].append(kind)
        return groups


def load_spec(path=SPEC_PATH):
    with open(path, "r") as handle:
        data = yaml.safe_load(handle)
    return Spec(data)


# ---------------------------------------------------------------------------
# Formatting helpers
#
# The committed sources are clang-format clean against duckdb/.clang-format
# (LLVM base, TabWidth 4, UseTab ForIndentation, ColumnLimit 120,
# AlignTrailingComments). This script reproduces that formatting directly in
# Python rather than shelling out to clang-format: the repo's .clang-format is a
# symlink into the duckdb submodule, which a plain `actions/checkout` leaves
# dangling, so a CI check that invoked clang-format would silently reformat with
# LLVM defaults. Pure Python keeps the sync check dependency-free apart from
# PyYAML, matching scripts/embed_sql_macros.py.
# ---------------------------------------------------------------------------

TAB_WIDTH = 4
COLUMN_LIMIT = 120


def bits(value, width):
    """Binary rendering of `value` in `width` bits, grouped as clang sees it."""
    return format(value, "0%db" % width)


def bit_comment_nibbles(code):
    """0x4C -> '0100 1100' — the 8-bit pattern the hpp comments carry."""
    text = bits(code, 8)
    return text[:4] + " " + text[4:]


def wrap_comment(comment, column):
    """Greedy word-wrap of `// <comment>` starting at `column`.

    Returns one string per physical line, each already carrying its `//`.
    Lengths are measured in characters, which matches clang-format's column
    accounting for the BMP text these comments use (the em dash in
    ORGANIZATION_CONTAINER is one column wide).
    """
    lines = []
    current = "//"
    for word in comment.split(" "):
        candidate = current + " " + word
        if current != "//" and column + len(candidate) > COLUMN_LIMIT:
            lines.append(current)
            current = "// " + word
        else:
            current = candidate
    lines.append(current)
    return lines


def render_trailing_comment(code, comment, column, base=0):
    """`code`, padded to `column`, then `comment` wrapped at the same column.

    `base` is the display width of the enclosing indent, so wrapping measures
    against the real ColumnLimit rather than a tab-relative one.
    """
    wrapped = wrap_comment(comment, base + column)
    lines = [code + " " * (column - len(code)) + wrapped[0]]
    lines.extend(" " * column + cont for cont in wrapped[1:])
    return lines


def render_constant_block(entries, indent=""):
    """Render one blank-line-delimited run of declarations as an alignment group.

    `entries` are dicts with `code` (everything up to and including the `;` or
    `,`), an optional `comment`, and an optional `split` (the declaration is
    broken after the `=`, as clang-format does when the statement and its
    comment cannot share a line).

    Entries without a comment, and split entries, stay out of the alignment
    group — AlignTrailingComments only lines up comments that are actually
    there, and a split entry aligns against its own continuation line.
    """
    aligned = [e for e in entries if e.get("comment") and not e.get("split")]
    column = max(len(e["code"]) for e in aligned) + 1 if aligned else 0
    base = len(indent.expandtabs(TAB_WIDTH))

    lines = []
    for entry in entries:
        if entry.get("split"):
            head, tail = entry["split"]
            lines.append(indent + head)
            lines.extend(
                indent + line
                for line in render_trailing_comment(
                    tail, entry["comment"], len(tail) + 1, base
                )
            )
        elif entry.get("comment"):
            lines.extend(
                indent + line
                for line in render_trailing_comment(
                    entry["code"], entry["comment"], column, base
                )
            )
        else:
            lines.append(indent + entry["code"])
    return lines


def comment_lines(text):
    """A spec doc string rendered as one `//` line per source line."""
    return ["// " + line if line else "//" for line in text.rstrip("\n").split("\n")]


# ---------------------------------------------------------------------------
# Region: semantic_type_constants  (src/include/semantic_types.hpp)
# ---------------------------------------------------------------------------


def render_semantic_type_constants(spec):
    """The 4 + 16 + 64 constexpr codes, with their bit-pattern comments.

    Every comment here is derived: a super kind shows its two high bits, a kind
    its four, and a semantic type its full byte, so the comments cannot drift
    from the codes the way hand-maintained ones did.
    """
    out = []

    out.append("// Super kinds (bits 6-7)")
    out.extend(
        render_constant_block(
            [
                {
                    "code": "constexpr uint8_t %s = 0x%02X;" % (sk["name"], sk["code"]),
                    "comment": "%sxx xxxx" % bits(sk["code"] >> 6, 2),
                }
                for sk in spec.super_kinds
            ]
        )
    )

    for super_kind_name in spec.declaration_order:
        super_code = spec.super_kind_code(super_kind_name)
        kinds = [k for k in spec.kinds if k["super_kind"] == super_kind_name]
        out.append("")
        out.append(
            "// Kinds within %s (%sss ssxx)" % (super_kind_name, bits(super_code >> 6, 2))
        )
        out.extend(
            render_constant_block(
                [
                    {
                        "code": "constexpr uint8_t %s = %s | 0x%02X;"
                        % (k["name"], super_kind_name, k["offset"]),
                        "comment": "%s xxxx" % bits(spec.kind_code(k["name"]) >> 4, 4),
                    }
                    for k in kinds
                ]
            )
        )

    for kind, types in spec.kind_groups_declared():
        kind_code = spec.kind_code(kind["name"])
        out.append("")
        out.append(
            "// ===== %s super types (%s ttxx) ====="
            % (kind["name"], bits(kind_code >> 4, 4))
        )
        entries = []
        for st in types:
            code = "constexpr uint8_t %s = %s | 0x%02X;" % (
                st["name"],
                kind["name"],
                st["offset"],
            )
            entry = {
                "code": code,
                "comment": "%s - %s"
                % (bit_comment_nibbles(spec.type_code(st["name"])), st["description"]),
            }
            if st.get("layout") == "break_after_assign":
                # clang-format cannot fit this declaration and its comment on
                # one line, so it breaks after the '=' and indents the value by
                # ContinuationIndentWidth. Recorded in the spec rather than
                # guessed, because the decision comes out of clang-format's
                # penalty model, not a rule this script can rederive.
                head, _, tail = code.partition(" = ")
                entry["split"] = (head + " =", "    " + tail)
            entries.append(entry)
        out.extend(render_constant_block(entries))

    return out


# ---------------------------------------------------------------------------
# Region: semantic_type_tables  (src/semantic_types.cpp)
# ---------------------------------------------------------------------------


def render_semantic_type_tables(spec):
    """GetSemanticTypeName/GetSuperKindName/GetKindName and their reverses."""
    out = []

    # -- GetSemanticTypeName ------------------------------------------------
    out.append("string GetSemanticTypeName(uint8_t semantic_type) {")
    out.append("\tswitch (semantic_type) {")
    for index, (kind, types) in enumerate(spec.kind_groups_declared()):
        if index:
            out.append("")
        out.append("\t// %s types" % kind["name"])
        for st in types:
            out.append("\tcase %s:" % st["name"])
            out.append('\t\treturn "%s";' % st["name"])
    out.append("")
    out.append("\tdefault:")
    out.append('\t\treturn "UNKNOWN_SEMANTIC_TYPE";')
    out.append("\t}")
    out.append("}")
    out.append("")

    # -- GetSuperKindName ---------------------------------------------------
    out.append("string GetSuperKindName(uint8_t super_kind) {")
    out.append("\tswitch (super_kind) {")
    for super_kind in spec.super_kinds:
        out.append("\tcase %s:" % super_kind["name"])
        out.append('\t\treturn "%s";' % super_kind["name"])
    out.append("\tdefault:")
    out.append('\t\treturn "UNKNOWN_SUPER_KIND";')
    out.append("\t}")
    out.append("}")
    out.append("")

    # -- GetKindName --------------------------------------------------------
    out.append("string GetKindName(uint8_t kind) {")
    out.append("\tswitch (kind) {")
    for index, (super_kind_name, kinds) in enumerate(spec.kind_groups_by_code()):
        if index:
            out.append("")
        out.append("\t// %s kinds" % super_kind_name)
        for kind in kinds:
            out.append("\tcase %s:" % kind["name"])
            out.append('\t\treturn "%s";' % kind["name"])
    out.append("")
    out.append("\tdefault:")
    out.append('\t\treturn "UNKNOWN_KIND";')
    out.append("\t}")
    out.append("}")
    out.append("")

    # -- GetSemanticTypeCode ------------------------------------------------
    out.append("// Reverse lookup - name to code")
    out.append("uint8_t GetSemanticTypeCode(const string &name) {")
    out.append("\t// Create a static map for efficient lookup")
    out.append("\tstatic unordered_map<string, uint8_t> name_to_code = {")
    for index, (kind, types) in enumerate(spec.kind_groups_declared()):
        if index:
            out.append("")
        out.append("\t    // %s types" % kind["name"])
        for st in types:
            out.append('\t    {"%s", %s},' % (st["name"], st["name"]))
    out.append("")
    out.append("\t};")
    out.append("")
    out.extend(_map_lookup_tail("name_to_code"))
    out.append("")

    # -- GetKindCode --------------------------------------------------------
    # One aligned braced-init list: the first element sits on the declaration
    # line and the rest align under it, which is how clang-format laid out the
    # hand-written table.
    decl = "static unordered_map<string, uint8_t> kind_to_code = {"
    pad = "\t" + " " * len(decl)
    items = []
    for super_kind_name, kinds in spec.kind_groups_by_code():
        if items:
            items.append("")
        items.append("// %s kinds" % super_kind_name)
        for kind in kinds:
            items.append('{"%s", %s},' % (kind["name"], kind["name"]))
    items[-1] = items[-1][:-1] + "};"  # last entry closes the list
    out.append("uint8_t GetKindCode(const string &name) {")
    out.append("\t" + decl + items[0])
    for item in items[1:]:
        out.append((pad + item) if item else "")
    out.append("")
    out.extend(_map_lookup_tail("kind_to_code"))
    out.append("")

    # -- GetSuperKindCode ---------------------------------------------------
    decl = "static unordered_map<string, uint8_t> super_kind_to_code = {"
    pad = "\t" + " " * len(decl)
    items = ['{"%s", %s},' % (sk["name"], sk["name"]) for sk in spec.super_kinds]
    items[-1] = items[-1][:-1] + "};"
    out.append("uint8_t GetSuperKindCode(const string &name) {")
    out.append("\t" + decl + items[0])
    for item in items[1:]:
        out.append(pad + item)
    out.append("")
    out.extend(_map_lookup_tail("super_kind_to_code"))

    return out


def _map_lookup_tail(map_name):
    return [
        "\tauto it = %s.find(name);" % map_name,
        "\tif (it != %s.end()) {" % map_name,
        "\t\treturn it->second;",
        "\t}",
        "\treturn 255; // Invalid code",
        "}",
    ]


# ---------------------------------------------------------------------------
# Flag-byte helpers
# ---------------------------------------------------------------------------


def flag_field(spec, name):
    for field in spec.flag_byte["fields"]:
        if field["name"] == name:
            return field
    raise ValueError("no flag field named %s" % name)


def runtime_selectable_flags(spec):
    """Flag names a runtime language config may name, in bit order.

    A bit field contributes its own name; an enum field contributes its
    selectable members. Masks and zero sentinels are excluded by their
    runtime_selectable: false.
    """
    names = []
    for field in spec.flag_byte["fields"]:
        if field["kind"] == "enum":
            for value in field["values"]:
                if value.get("runtime_selectable"):
                    names.append(value["name"])
        elif field.get("runtime_selectable"):
            names.append(field["name"])
    return names


def name_role_values(spec):
    """The NAME_ROLE members ordered by their 2-bit role value."""
    field = flag_field(spec, "NAME_ROLE")
    return sorted(field["values"], key=lambda v: v["role_value"])


def strategy_enum(spec, enum_name):
    for entry in spec.strategies:
        if entry["enum"] == enum_name:
            return entry
    raise ValueError("no strategy enum named %s" % enum_name)


def strategy_label(value):
    """The ast_type_map() spelling of a strategy: lowercase unless overridden."""
    return value.get("label", value["name"].lower())


# ---------------------------------------------------------------------------
# Region: extraction_strategy_enums  (src/include/node_config.hpp)
# ---------------------------------------------------------------------------


def render_extraction_strategy_enums(spec):
    """ExtractionStrategy and NativeExtractionStrategy.

    explicit_values decides which enumerators carry an `= N`: `all` spells out
    every value (ExtractionStrategy, whose numbers the .def files and the
    runtime config both depend on), `endpoints` spells out only the first and
    last (NativeExtractionStrategy, where the middle is a contiguous run and
    CUSTOM is pinned at 255).
    """
    out = []
    for index, enum in enumerate(spec.strategies):
        if index:
            out.append("")
        out.append("// %s" % enum["header_comment"])
        out.append("enum class %s : %s {" % (enum["enum"], enum["underlying"]))
        values = enum["values"]
        entries = []
        for position, value in enumerate(values):
            last = position == len(values) - 1
            if enum["explicit_values"] == "all" or "value" in value:
                code = "%s = %d" % (value["name"], value["value"])
            else:
                code = value["name"]
            entries.append(
                {"code": code + ("" if last else ","), "comment": value["doc"]}
            )
        out.extend(render_constant_block(entries, indent="\t"))
        out.append("};")
    return out


# ---------------------------------------------------------------------------
# Region: node_flag_constants  (src/include/node_config.hpp)
# ---------------------------------------------------------------------------


def render_node_flag_constants(spec):
    """The flag byte's bit constants, in bit order, with the mask and aliases.

    An enum field emits its mask constant first, then its members, as one
    alignment group; a plain bit field emits a single constant with no trailing
    comment. The deprecated aliases keep their own block at the end.
    """
    out = []
    for index, field in enumerate(spec.flag_byte["fields"]):
        if index:
            out.append("")
        out.extend(comment_lines(field["summary"]))
        if field["kind"] == "enum":
            entries = [
                {
                    "code": "constexpr uint8_t %s = 0x%02X;"
                    % (field["mask_name"], field["mask"]),
                    "comment": field["mask_doc"],
                }
            ]
            for value in sorted(field["values"], key=lambda v: v["role_value"]):
                entries.append(
                    {
                        "code": "constexpr uint8_t %s = 0x%02X;"
                        % (value["name"], value["value"]),
                        "comment": "%s = %s"
                        % (
                            bits(value["role_value"], len(field["bits"])),
                            value["doc"],
                        ),
                    }
                )
            out.extend(render_constant_block(entries))
        else:
            out.append(
                "constexpr uint8_t %s = 0x%02X;" % (field["name"], field["value"])
            )

    for derived in spec.flag_byte["derived"]:
        out.append("")
        out.extend(comment_lines(derived["summary"]))
        out.append(
            "constexpr uint8_t %s = 0x%02X;" % (derived["name"], derived["value"])
        )

    out.append("")
    out.extend(comment_lines(spec.flag_byte["deprecated_aliases_summary"]))
    out.extend(
        render_constant_block(
            [
                {
                    "code": "constexpr uint8_t %s = %s;"
                    % (alias["name"], alias["value"]),
                    "comment": alias.get("doc"),
                }
                for alias in spec.flag_byte["deprecated_aliases"]
            ]
        )
    )
    return out


# ---------------------------------------------------------------------------
# Region: name_strategy_entries  (src/language_config_json.cpp)
# ---------------------------------------------------------------------------


def render_name_strategy_entries(spec):
    """The runtime-selectable name strategies, as a constexpr table.

    CUSTOM is excluded (runtime_selectable: false): it needs native C++ logic.
    The static_assert below this block in the source ties the entry count to
    ExtractionStrategy::CUSTOM, so dropping a strategy here fails the build.
    """
    enum = strategy_enum(spec, "ExtractionStrategy")
    out = ["constexpr NameStrategyEntry NAME_STRATEGY_ENTRIES[] = {"]
    for value in enum["values"]:
        if not value.get("runtime_selectable"):
            continue
        out.append(
            '    {"%s", ExtractionStrategy::%s},' % (value["name"], value["name"])
        )
    out.append("};")
    return out


# ---------------------------------------------------------------------------
# Region: flag_name_table  (src/language_config_json.cpp)
# ---------------------------------------------------------------------------


def render_flag_name_table(spec):
    namespace = spec.flag_byte["namespace"]
    out = ["\tstatic const unordered_map<string, uint8_t> flag_names = {"]
    for name in runtime_selectable_flags(spec):
        out.append('\t    {"%s", %s::%s},' % (name, namespace, name))
    out.append("\t};")
    return out


# ---------------------------------------------------------------------------
# Region: type_map_name_tables  (src/ast_type_map_function.cpp)
# ---------------------------------------------------------------------------


def render_type_map_name_tables(spec):
    """The lowercased name_role / name_strategy spellings ast_type_map() emits."""
    roles = name_role_values(spec)
    none_label = roles[0]["label"]

    out = ["static string GetNameRoleString(uint8_t flags) {"]
    out.append(
        "\tuint8_t role = (flags & %s::%s) >> %d;"
        % (
            spec.flag_byte["namespace"],
            flag_field(spec, "NAME_ROLE")["mask_name"],
            flag_field(spec, "NAME_ROLE")["bits"][0],
        )
    )
    out.append("\tswitch (role) {")
    for value in roles:
        out.append("\tcase %d:" % value["role_value"])
        out.append('\t\treturn "%s";' % value["label"])
    out.append("\tdefault:")
    out.append('\t\treturn "%s";' % none_label)
    out.append("\t}")
    out.append("}")
    out.append("")

    out.append("static string GetExtractionStrategyName(ExtractionStrategy strategy) {")
    out.append("\tswitch (strategy) {")
    for value in strategy_enum(spec, "ExtractionStrategy")["values"]:
        out.append("\tcase ExtractionStrategy::%s:" % value["name"])
        out.append('\t\treturn "%s";' % strategy_label(value))
    out.append("\tdefault:")
    out.append('\t\treturn "unknown";')
    out.append("\t}")
    out.append("}")
    return out


# ---------------------------------------------------------------------------
# Regions
# ---------------------------------------------------------------------------

REGIONS = [
    (
        "extraction_strategy_enums",
        "src/include/node_config.hpp",
        render_extraction_strategy_enums,
    ),
    ("node_flag_constants", "src/include/node_config.hpp", render_node_flag_constants),
    (
        "semantic_type_constants",
        "src/include/semantic_types.hpp",
        render_semantic_type_constants,
    ),
    ("semantic_type_tables", "src/semantic_types.cpp", render_semantic_type_tables),
    (
        "name_strategy_entries",
        "src/language_config_json.cpp",
        render_name_strategy_entries,
    ),
    ("flag_name_table", "src/language_config_json.cpp", render_flag_name_table),
    (
        "type_map_name_tables",
        "src/ast_type_map_function.cpp",
        render_type_map_name_tables,
    ),
]


def region_text(spec, region_id):
    for rid, _path, renderer in REGIONS:
        if rid == region_id:
            return "\n".join(renderer(spec)) + "\n"
    sys.exit("unknown region %r (known: %s)" % (region_id, ", ".join(r[0] for r in REGIONS)))


def _find_marker(lines, text, region_id):
    """The index of the single line whose content is exactly `text`.

    Compared after stripping, so a marker may be indented to match the code it
    wraps (FlagNameTable's block sits inside a function body).
    """
    hits = [i for i, line in enumerate(lines) if line.strip() == text]
    if len(hits) != 1:
        sys.exit(
            "region %r: expected exactly one %r line, found %d"
            % (region_id, text, len(hits))
        )
    return hits[0]


def replace_region(source, region_id, body):
    """Swap the lines between a region's markers, keeping the markers in place."""
    begin = BEGIN_FMT.format(region=region_id)
    end = END_FMT.format(region=region_id)
    lines = source.splitlines(keepends=True)
    begin_index = _find_marker(lines, begin, region_id)
    end_index = _find_marker(lines, end, region_id)
    if end_index <= begin_index:
        sys.exit("region %r: END marker precedes BEGIN marker" % region_id)

    # The fixed note lines directly below BEGIN belong to the header, not the body.
    body_start = begin_index + 1
    while body_start < end_index and lines[body_start].strip() in (
        MARKER_NOTE,
        MARKER_NOTE2,
    ):
        body_start += 1

    return "".join(lines[:body_start]) + body + "".join(lines[end_index:])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--check",
        action="store_true",
        help="exit non-zero if a committed region differs from the spec",
    )
    parser.add_argument(
        "--emit",
        metavar="REGION",
        help="print one region's generated text to stdout and exit",
    )
    args = parser.parse_args()

    spec = load_spec()

    if args.emit:
        sys.stdout.write(region_text(spec, args.emit))
        return 0

    stale = []
    for region_id, rel_path, _renderer in REGIONS:
        path = PROJECT_ROOT / rel_path
        source = path.read_text()
        updated = replace_region(source, region_id, region_text(spec, region_id))
        if updated == source:
            continue
        if args.check:
            stale.append(rel_path)
        else:
            path.write_text(updated)
            print("Updated %s (%s)" % (rel_path, region_id))

    if stale:
        sys.stderr.write(
            "taxonomy tables are out of sync with spec/taxonomy/taxonomy.yaml: %s\n"
            % ", ".join(stale)
        )
        return 1
    if args.check:
        print("taxonomy tables are in sync with spec/taxonomy/taxonomy.yaml")
    return 0


if __name__ == "__main__":
    sys.exit(main())
