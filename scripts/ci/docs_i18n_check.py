#!/usr/bin/env python3
"""Check VitePress locale pairs and page routes without walking dependencies."""

from pathlib import Path
import argparse
from html.parser import HTMLParser
import re
import subprocess
import sys
from urllib.parse import unquote, urlsplit

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--dist", type=Path, help="Also validate generated HTML language and metadata")
args = parser.parse_args()

ROOT = Path(__file__).resolve().parents[2]
names = subprocess.check_output(
    ["git", "ls-files", "--cached", "--others", "--exclude-standard", "-z"], cwd=ROOT
).decode().split("\0")
pages = {
    name for name in names
    if name.startswith("docs/")
    and not name.startswith(("docs/public/", "docs/.vitepress/"))
    and not any(part in {"node_modules", ".pnpm-store"} for part in Path(name).parts)
    and name.endswith(".md") and (ROOT / name).is_file()
}
errors = []
chinese = {name.removeprefix("docs/") for name in pages if not name.startswith("docs/en/")}
english = {name.removeprefix("docs/en/") for name in pages if name.startswith("docs/en/")}
for name in sorted(chinese ^ english):
    errors.append(f"Missing locale counterpart: {name}")

def check_route(page, label, value):
    route = urlsplit(value).path
    if not route.startswith("/") or route.startswith("//"):
        return
    route = unquote(route)
    target = ROOT / "docs" / route.lstrip("/")
    candidates = [target, target.with_suffix(".md"), target / "index.md"]
    if not any(p.is_file() for p in candidates):
        errors.append(f"{page}: missing page route {value}")
    expected_english = page.startswith("docs/en/")
    actual_english = route.startswith("/en/")
    # A deliberately labelled link to an extended original-language reference
    # is allowed; navigation and ordinary page links must stay in their locale.
    explicit_language = "Chinese" in label or "中文" in label or "English" in label
    if expected_english != actual_english and not explicit_language:
        errors.append(f"{page}: route crosses locale without a language label: {value}")

for page in sorted(pages):
    if page.endswith((".zh.md", ".en.md")):
        errors.append(f"Use locale directories instead of language suffixes: {page}")
    source = (ROOT / page).read_text()
    # Ignore fenced examples: example URLs are not site navigation.
    source = re.sub(r"^(`{3,}|~{3,}).*?^\1[^\n]*$", "", source, flags=re.M | re.S)
    for match in re.finditer(r"\[([^\]]*)\]\((/[^\s)]*)\)", source):
        check_route(page, match[1], match[2])
    if source.startswith("---\n"):
        frontmatter = source.split("---", 2)[1]
        for match in re.finditer(r"^\s*(?:link|prev|next):\s*['\"]?(/[^\s'\"]+)", frontmatter, re.M):
            check_route(page, "navigation", match[1])

if args.dist:
    class PageMetadata(HTMLParser):
        def __init__(self):
            super().__init__()
            self.lang = None
            self.description = None

        def handle_starttag(self, tag, attrs):
            attrs = dict(attrs)
            if tag == "html":
                self.lang = attrs.get("lang")
            elif tag == "meta" and attrs.get("name") == "description":
                self.description = attrs.get("content")

    for page in sorted(pages):
        relative = Path(page.removeprefix("docs/")).with_suffix(".html")
        output = args.dist / relative
        if not output.is_file():
            errors.append(f"Missing generated locale page: {output}")
            continue
        metadata = PageMetadata()
        metadata.feed(output.read_text())
        expected = "en-US" if page.startswith("docs/en/") else "zh-CN"
        if metadata.lang != expected:
            errors.append(f"{output}: html lang={metadata.lang!r}, expected {expected}")
        if not metadata.description:
            errors.append(f"{output}: missing locale page description")
        elif expected == "en-US" and re.search(r"[\u4e00-\u9fff]", metadata.description):
            errors.append(f"{output}: English page inherited a Chinese description")

if errors:
    print("\n".join(errors), file=sys.stderr)
    sys.exit(1)
print(f"VitePress locales: {len(chinese)} Chinese/English page pairs; page routes passed")
if args.dist:
    print(f"Generated locale HTML language and descriptions passed: {len(pages)} pages")
