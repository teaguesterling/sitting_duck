#!/usr/bin/env python3
"""
Generate language reference documentation from .def files.

This script parses the Doxygen-style comments in src/language_configs/*_types.def
files and generates markdown documentation for each language's node types.

Usage:
    python generate_language_reference.py [output_dir]

The output_dir defaults to docs/reference/languages/
"""

import re
import os
import sys
from pathlib import Path
from dataclasses import dataclass, field
from typing import Optional


@dataclass
class NodeType:
    """A single DEF_TYPE entry."""
    raw_type: str
    semantic_type: str
    name_extraction: str
    native_extraction: str
    flags: str
    brief: Optional[str] = None
    group: Optional[str] = None


@dataclass
class DefGroup:
    """A @defgroup section."""
    name: str
    title: str
    brief: str
    description: str = ""
    node_types: list = field(default_factory=list)


@dataclass
class LanguageDoc:
    """Documentation for a single language."""
    language: str
    file_path: str
    file_brief: str = ""
    file_details: str = ""
    characteristics: str = ""
    groups: list = field(default_factory=list)
    ungrouped_types: list = field(default_factory=list)


def parse_def_file(filepath: Path) -> LanguageDoc:
    """Parse a .def file and extract documentation."""
    content = filepath.read_text()

    # Extract language name from filename (e.g., python_types.def -> Python)
    lang_name = filepath.stem.replace('_types', '').replace('_', ' ').title()

    doc = LanguageDoc(language=lang_name, file_path=str(filepath))

    # Extract file-level documentation
    file_doc_match = re.search(r'/\*\*\s*\n\s*\*\s*@file.*?\*/', content, re.DOTALL)
    if file_doc_match:
        file_doc = file_doc_match.group(0)

        # Extract @brief
        brief_match = re.search(r'@brief\s+(.+?)(?=\n\s*\*\s*(?:@|\n))', file_doc, re.DOTALL)
        if brief_match:
            doc.file_brief = clean_comment_text(brief_match.group(1))

        # Extract @details
        details_match = re.search(r'@details\s+(.+?)(?=\n\s*\*\s*##|\n\s*\*\s*@)', file_doc, re.DOTALL)
        if details_match:
            doc.file_details = clean_comment_text(details_match.group(1))

        # Extract language characteristics section
        chars_match = re.search(r'##\s*\w+\s+Language\s+Characteristics\s*\n(.+?)(?=\n\s*\*\s*@see|\*/)', file_doc, re.DOTALL)
        if chars_match:
            doc.characteristics = clean_comment_text(chars_match.group(1))

    # Find all defgroups
    current_group = None
    current_brief = None

    lines = content.split('\n')
    i = 0
    while i < len(lines):
        line = lines[i]

        # Look for @defgroup
        defgroup_match = re.search(r'@defgroup\s+(\w+)\s+(.+)', line)
        if defgroup_match:
            group_name = defgroup_match.group(1)
            group_title = defgroup_match.group(2).strip()

            # Extract @brief from next lines
            brief = ""
            desc = ""
            j = i + 1
            while j < len(lines):
                next_line = lines[j]
                if '@brief' in next_line:
                    brief_match = re.search(r'@brief\s+(.+)', next_line)
                    if brief_match:
                        brief = brief_match.group(1).strip()
                elif '@{' in next_line:
                    break
                elif '* ' in next_line and not next_line.strip().startswith('* @'):
                    # Description line
                    desc_text = re.sub(r'^\s*\*\s*', '', next_line)
                    if desc_text.strip():
                        desc += desc_text.strip() + " "
                j += 1

            current_group = DefGroup(
                name=group_name,
                title=group_title,
                brief=brief,
                description=desc.strip()
            )
            doc.groups.append(current_group)

        # Look for /** @brief before DEF_TYPE (single-line brief)
        if line.strip().startswith('///') and '@brief' in line:
            brief_match = re.search(r'@brief\s+(.+)', line)
            if brief_match:
                current_brief = brief_match.group(1).strip()
        elif line.strip().startswith('/**') and i + 1 < len(lines):
            # Multi-line brief
            j = i
            brief_text = ""
            while j < len(lines) and '*/' not in lines[j]:
                if '@brief' in lines[j]:
                    brief_match = re.search(r'@brief\s+(.+)', lines[j])
                    if brief_match:
                        brief_text = brief_match.group(1).strip()
                j += 1
            if brief_text:
                current_brief = brief_text

        # Look for DEF_TYPE
        def_match = re.match(r'\s*DEF_TYPE\s*\(\s*"([^"]+)"\s*,\s*([^,]+)\s*,\s*([^,]+)\s*,\s*([^,]+)\s*,\s*([^)]+)\s*\)', line)
        if def_match:
            node = NodeType(
                raw_type=def_match.group(1),
                semantic_type=def_match.group(2).strip(),
                name_extraction=def_match.group(3).strip(),
                native_extraction=def_match.group(4).strip(),
                flags=def_match.group(5).strip(),
                brief=current_brief,
                group=current_group.name if current_group else None
            )

            if current_group:
                current_group.node_types.append(node)
            else:
                doc.ungrouped_types.append(node)

            current_brief = None

        # Look for @} which ends a group
        if '@}' in line:
            current_group = None

        i += 1

    return doc


def clean_comment_text(text: str) -> str:
    """Clean up comment text by removing * prefixes and normalizing whitespace."""
    lines = text.split('\n')
    cleaned = []
    for line in lines:
        # Remove leading * from comment lines
        line = re.sub(r'^\s*\*\s?', '', line)
        cleaned.append(line)
    return '\n'.join(cleaned).strip()


def format_semantic_type(semantic_type: str) -> str:
    """Format semantic type for display."""
    # Simplify common patterns
    sem = semantic_type
    sem = sem.replace('SemanticRefinements::', '')
    sem = sem.replace('ASTSemanticType::', '')
    return sem


def generate_markdown(doc: LanguageDoc) -> str:
    """Generate markdown documentation for a language."""
    md = []

    # Title
    md.append(f"# {doc.language} Node Types")
    md.append("")

    # File brief
    if doc.file_brief:
        md.append(f"> {doc.file_brief}")
        md.append("")

    # Language characteristics
    if doc.characteristics:
        md.append("## Language Characteristics")
        md.append("")
        md.append(doc.characteristics)
        md.append("")

    # Table of contents
    if doc.groups:
        md.append("## Node Categories")
        md.append("")
        for group in doc.groups:
            anchor = group.title.lower().replace(' ', '-').replace('/', '-')
            md.append(f"- [{group.title}](#{anchor})")
        md.append("")

    # Each group
    for group in doc.groups:
        md.append(f"## {group.title}")
        md.append("")

        if group.brief:
            md.append(f"{group.brief}")
            md.append("")

        if group.description:
            md.append(group.description)
            md.append("")

        if group.node_types:
            md.append("| Node Type | Semantic Type | Name Extraction | Description |")
            md.append("|-----------|---------------|-----------------|-------------|")

            for node in group.node_types:
                sem_type = format_semantic_type(node.semantic_type)
                brief = node.brief or ""
                md.append(f"| `{node.raw_type}` | {sem_type} | {node.name_extraction} | {brief} |")

            md.append("")

    # Ungrouped types
    if doc.ungrouped_types:
        md.append("## Other Node Types")
        md.append("")
        md.append("| Node Type | Semantic Type | Name Extraction | Description |")
        md.append("|-----------|---------------|-----------------|-------------|")

        for node in doc.ungrouped_types:
            sem_type = format_semantic_type(node.semantic_type)
            brief = node.brief or ""
            md.append(f"| `{node.raw_type}` | {sem_type} | {node.name_extraction} | {brief} |")

        md.append("")

    # Footer
    md.append("---")
    md.append("")
    md.append(f"*Generated from `{Path(doc.file_path).name}`*")
    md.append("")

    return '\n'.join(md)


def generate_index(languages: list) -> str:
    """Generate index page for all languages."""
    md = []

    md.append("# Language Node Type Reference")
    md.append("")
    md.append("This reference documents all AST node types supported by Sitting Duck,")
    md.append("organized by programming language. Each page shows:")
    md.append("")
    md.append("- **Node Type**: The tree-sitter node type string")
    md.append("- **Semantic Type**: Universal semantic classification")
    md.append("- **Name Extraction**: Strategy for extracting identifiers")
    md.append("- **Description**: What the node represents")
    md.append("")
    md.append("## Languages")
    md.append("")

    # Map from filename prefix to display info
    # Format: prefix -> (display_name, category)
    lang_info = {
        'javascript': ('JavaScript', 'Web'),
        'typescript': ('TypeScript', 'Web'),
        'html': ('HTML', 'Web'),
        'css': ('CSS', 'Web'),
        'c': ('C', 'Systems'),
        'cpp': ('C++', 'Systems'),
        'go': ('Go', 'Systems'),
        'rust': ('Rust', 'Systems'),
        'zig': ('Zig', 'Systems'),
        'python': ('Python', 'Scripting'),
        'ruby': ('Ruby', 'Scripting'),
        'php': ('PHP', 'Scripting'),
        'lua': ('Lua', 'Scripting'),
        'r': ('R', 'Scripting'),
        'bash': ('Bash', 'Scripting'),
        'java': ('Java', 'Enterprise & Mobile'),
        'csharp': ('C#', 'Enterprise & Mobile'),
        'kotlin': ('Kotlin', 'Enterprise & Mobile'),
        'swift': ('Swift', 'Enterprise & Mobile'),
        'dart': ('Dart', 'Enterprise & Mobile'),
        'scala': ('Scala', 'Enterprise & Mobile'),
        'hcl': ('HCL (Terraform)', 'Infrastructure'),
        'json': ('JSON', 'Infrastructure'),
        'toml': ('TOML', 'Infrastructure'),
        'graphql': ('GraphQL', 'Infrastructure'),
        'yaml': ('YAML', 'Infrastructure'),
        'markdown': ('Markdown', 'Documentation'),
        'sql': ('SQL', 'Documentation'),
        'fsharp': ('F#', 'Functional'),
        'haskell': ('Haskell', 'Functional'),
        'julia': ('Julia', 'Scientific'),
    }

    # Build category -> languages mapping
    categories = {}
    for lang_name, filename in languages:
        prefix = filename.replace('_types.def', '')
        if prefix in lang_info:
            display_name, category = lang_info[prefix]
        else:
            display_name = lang_name
            category = 'Other'

        if category not in categories:
            categories[category] = []
        categories[category].append((display_name, prefix))

    # Output in desired order
    category_order = [
        'Web', 'Systems', 'Scripting', 'Enterprise & Mobile',
        'Infrastructure', 'Documentation', 'Functional', 'Scientific', 'Other'
    ]

    for category in category_order:
        if category not in categories:
            continue
        md.append(f"### {category}")
        md.append("")
        for display_name, prefix in sorted(categories[category]):
            md.append(f"- [{display_name}]({prefix}.md)")
        md.append("")

    md.append("---")
    md.append("")
    md.append("*This documentation is auto-generated from the `.def` files in `src/language_configs/`*")
    md.append("")

    return '\n'.join(md)


def main():
    # Determine paths
    script_dir = Path(__file__).parent
    project_root = script_dir.parent.parent

    def_dir = project_root / 'src' / 'language_configs'
    output_dir = Path(sys.argv[1]) if len(sys.argv) > 1 else script_dir.parent / 'reference' / 'languages'

    # Create output directory
    output_dir.mkdir(parents=True, exist_ok=True)

    print(f"Parsing .def files from: {def_dir}")
    print(f"Writing documentation to: {output_dir}")

    # Find all .def files
    def_files = sorted(def_dir.glob('*_types.def'))

    if not def_files:
        print(f"No .def files found in {def_dir}")
        sys.exit(1)

    languages = []

    for def_file in def_files:
        print(f"  Processing {def_file.name}...")

        try:
            doc = parse_def_file(def_file)
            markdown = generate_markdown(doc)

            # Write output
            output_file = output_dir / f"{def_file.stem.replace('_types', '')}.md"
            output_file.write_text(markdown)

            languages.append((doc.language, def_file.name))

            # Count stats
            total_types = sum(len(g.node_types) for g in doc.groups) + len(doc.ungrouped_types)
            print(f"    -> {len(doc.groups)} groups, {total_types} node types")

        except Exception as e:
            print(f"    ERROR: {e}")
            raise

    # Generate index
    print("  Generating index...")
    index_md = generate_index(languages)
    (output_dir / 'index.md').write_text(index_md)

    print(f"\nGenerated documentation for {len(languages)} languages")


if __name__ == '__main__':
    main()
