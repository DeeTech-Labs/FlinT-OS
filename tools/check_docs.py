"""Check repository-local Markdown links and image paths without network access."""

from __future__ import annotations

import re
import sys
from pathlib import Path
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1]
LINK = re.compile(r'\]\(([^\s)]+)(?:\s+"[^"]*")?\)|\b(?:href|src)=["\']([^"\']+)["\']')


def anchors(text: str) -> set[str]:
    result = set(re.findall(r'\b(?:id|name)=["\']([^"\']+)["\']', text))
    counts: dict[str, int] = {}
    for heading in re.findall(r"^#{1,6}\s+(.+)$", text, re.MULTILINE):
        slug = re.sub(r"[^\w\- ]", "", heading.lower()).replace(" ", "-")
        count = counts.get(slug, 0)
        counts[slug] = count + 1
        result.add(f"{slug}-{count}" if count else slug)
    return result


def check() -> list[str]:
    errors = []
    documents = sorted(ROOT.glob("*.md")) + sorted((ROOT / "docs").rglob("*.md"))
    for source in documents:
        # Examples inside fenced blocks are not document navigation.
        text = re.sub(r"^```.*?^```[^\n]*", "", source.read_text(encoding="utf-8"),
                      flags=re.MULTILINE | re.DOTALL)
        for match in LINK.finditer(text):
            url = next(value for value in match.groups() if value is not None)
            parts = urlsplit(url)
            if parts.scheme or parts.netloc:
                continue
            target = (source.parent / unquote(parts.path)).resolve() if parts.path else source
            if not target.exists():
                errors.append(f"{source.relative_to(ROOT)}: missing {url}")
            elif parts.fragment and target.suffix == ".md":
                if unquote(parts.fragment) not in anchors(target.read_text(encoding="utf-8")):
                    errors.append(f"{source.relative_to(ROOT)}: missing anchor {url}")
    return errors


if __name__ == "__main__":
    errors = check()
    if errors:
        print("\n".join(errors))
        sys.exit(1)
    print("Repository Markdown links and images: OK")
