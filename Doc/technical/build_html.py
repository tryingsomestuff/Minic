#!/usr/bin/env python3
"""Combine this directory's Markdown documentation into one static HTML page."""

from __future__ import annotations

import argparse
import html
from pathlib import Path
import re
from urllib.parse import quote, unquote, urlsplit, urlunsplit
from xml.etree.ElementTree import Element

try:
    import markdown
    from markdown.extensions import Extension
    from markdown.extensions.toc import slugify
    from markdown.inlinepatterns import InlineProcessor
    from markdown.preprocessors import Preprocessor
    from markdown.treeprocessors import Treeprocessor
    from markdown.util import AtomicString
except ImportError as error:
    raise SystemExit(
        "Python-Markdown is required. Install it with: python -m pip install Markdown"
    ) from error


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
GITHUB_URL = "https://github.com/tryingsomestuff/Minic/blob/master/"
RAW_URL = "https://raw.githubusercontent.com/tryingsomestuff/Minic/master/"
MATHJAX_URL = "https://cdn.jsdelivr.net/npm/mathjax@3.2.2/es5/tex-svg.js"
MERMAID_URL = "https://cdn.jsdelivr.net/npm/mermaid@11.12.0/dist/mermaid.esm.min.mjs"


STYLE = """
:root { color-scheme: light dark; --accent: #0969da; --border: #aaa5; }
* { box-sizing: border-box; }
body { margin: 0; font: 16px/1.65 system-ui, sans-serif; }
a { color: var(--accent); text-decoration: none; }
a:hover { text-decoration: underline; }
nav { position: fixed; inset: 0 auto 0 0; width: 270px;
      padding: 24px; overflow-y: auto; border-right: 1px solid var(--border); }
nav strong { display: block; margin-bottom: 16px; }
nav a { display: block; margin: 12px 0; }
main { max-width: 1200px; margin-left: 270px; padding: 32px 48px; }
article { padding-bottom: 48px; border-bottom: 1px solid var(--border); }
h1, h2, h3, h4 { line-height: 1.3; scroll-margin-top: 24px; }
h1 { margin-top: 40px; }
pre { overflow-x: auto; padding: 16px; background: #8881;
      border: 1px solid var(--border); border-radius: 6px; }
code { font-family: ui-monospace, monospace; font-size: .9em; }
:not(pre) > code { background: #8881; padding: 2px 4px; border-radius: 4px; }
table { display: block; overflow-x: auto; border-collapse: collapse;
        margin: 20px 0; font-size: .95em; }
th, td { padding: 10px 14px; border: 1px solid var(--border); text-align: left; }
th { background: #8881; }
blockquote { margin-left: 0; padding-left: 20px;
             border-left: 4px solid var(--border); }
img { max-width: 100%; }
.math-display, .mermaid-diagram { overflow-x: auto; margin: 20px 0; }
.math-display { white-space: pre-wrap; }
.mermaid-diagram svg { display: block; max-width: 100%; height: auto; margin: auto; }
.diagram-source { margin: 12px 0 24px; }
.diagram-source summary { cursor: pointer; }
.render-warning { padding: 12px; border: 1px solid var(--border); }
@media (prefers-color-scheme: dark) { :root { --accent: #79b8ff; } }
@media (max-width: 850px) {
  nav { position: static; width: auto; border-right: 0;
        border-bottom: 1px solid var(--border); }
  main { margin-left: 0; padding: 20px; }
}
@media print {
  nav { display: none; }
  main { margin: 0; padding: 0; max-width: none; }
  pre, table { overflow: visible; }
  article { break-before: page; }
  article:first-child { break-before: auto; }
}
"""


RENDER_SCRIPT = """
<script>
function renderingWarning(message, error, target = document.querySelector("main")) {
  console.error(message, error);
  const notice = document.createElement("p");
  notice.className = "render-warning";
  notice.setAttribute("role", "status");
  notice.textContent = message;
  target.prepend(notice);
}

const formulas = document.querySelectorAll(".math");
if (formulas.length) {
  window.MathJax = {
    tex: { inlineMath: [["\\\\(", "\\\\)"]], displayMath: [["$$", "$$"]] },
    svg: { fontCache: "local" },
    startup: { typeset: false }
  };
  const script = document.createElement("script");
  script.src = MATHJAX_URL;
  script.onload = () => {
    MathJax.startup.promise
      .then(() => MathJax.typesetPromise(Array.from(formulas)))
      .catch(error => renderingWarning(
        "Math rendering failed; LaTeX source remains available.", error));
  };
  script.onerror = error => renderingWarning(
    "MathJax could not load. Connect to the internet for rendered equations; "
      + "LaTeX source remains readable.", error);
  document.head.append(script);
}
</script>
<script type="module">
const blocks = document.querySelectorAll("pre > code.language-mermaid");
if (blocks.length) {
  try {
    const { default: mermaid } = await import(MERMAID_URL);
    mermaid.initialize({
      startOnLoad: false,
      securityLevel: "strict",
      theme: matchMedia("(prefers-color-scheme: dark)").matches ? "dark" : "default"
    });
    for (const [index, code] of Array.from(blocks).entries()) {
      const pre = code.parentElement;
      try {
        const { svg } = await mermaid.render(`minic-diagram-${index}`, code.textContent);
        const diagram = document.createElement("div");
        diagram.className = "mermaid-diagram";
        diagram.innerHTML = svg;
        const source = document.createElement("details");
        source.className = "diagram-source";
        const summary = document.createElement("summary");
        summary.textContent = "Diagram source (Mermaid)";
        source.append(summary);
        pre.replaceWith(diagram, source);
        source.append(pre);
      } catch (error) {
        renderingWarning("A Mermaid diagram could not render; its source is shown.",
          error, pre.parentElement);
      }
    }
  } catch (error) {
    renderingWarning(
      "Mermaid could not load. Connect to the internet for diagrams; "
        + "diagram source remains readable.", error);
  }
}
</script>
"""


class MathBlocks(Preprocessor):
    def run(self, lines):
        result = []
        formula = None
        for line in lines:
            if re.fullmatch(r" {0,3}\$\$[ \t]*", line):
                if formula is None:
                    formula = []
                else:
                    tex = "\n".join(formula).strip()
                    if not tex:
                        raise ValueError("Empty $$ display-math block.")
                    placeholder = self.md.htmlStash.store(
                        '<div class="math math-display">$$\n'
                        + html.escape(tex) + "\n$$</div>"
                    )
                    result.extend(["", placeholder, ""])
                    formula = None
            elif formula is None:
                result.append(line)
            else:
                formula.append(line)
        if formula is not None:
            raise ValueError("Unclosed $$ display-math block.")
        return result


class MathInline(InlineProcessor):
    def handleMatch(self, match, data):
        element = Element("span", {"class": "math"})
        element.text = AtomicString("\\(" + match.group(1) + "\\)")
        return element, match.start(0), match.end(0)


class TechnicalNotation(Extension):
    def extendMarkdown(self, renderer):
        # Stash fenced code first; preserve TeX before Markdown's HTML/inline passes.
        renderer.preprocessors.register(MathBlocks(renderer), "math_blocks", 24)
        renderer.inlinePatterns.register(
            MathInline(r"(?<!\\)\$(?!\$)([^\n$]+?)(?<!\\)\$(?!\$)", renderer),
            "math_inline", 185,
        )


def repository_target(source: Path, value: str):
    parts = urlsplit(value)
    if value.startswith(GITHUB_URL):
        relative = unquote(parts.path[len(urlsplit(GITHUB_URL).path):])
        target = (REPOSITORY_ROOT / relative).resolve()
    elif parts.scheme or parts.netloc or parts.path.startswith("/"):
        return None
    else:
        target = (
            (source.parent / unquote(parts.path)).resolve()
            if parts.path else source
        )
    try:
        target.relative_to(REPOSITORY_ROOT)
    except ValueError as error:
        raise ValueError(f"Documentation link points outside the repository: {value}") from error
    return target


class DocumentLinks(Treeprocessor):
    def __init__(self, renderer, source, chapter_ids):
        super().__init__(renderer)
        self.source = source
        self.chapter_ids = chapter_ids
        self.chapter_id = chapter_ids[source]
        self.title = source.stem.replace("-", " ").title()

    def run(self, root):
        heading = root.find(".//h1")
        if heading is not None:
            self.title = "".join(heading.itertext())

        for element in root.iter():
            if "id" in element.attrib:
                element.set("id", f"{self.chapter_id}-{element.get('id')}")
            for attribute in ("href", "src"):
                value = element.get(attribute)
                if value is not None:
                    element.set(attribute, self.rewrite(value, raw=attribute == "src"))
        return root

    def rewrite(self, value, raw=False):
        parts = urlsplit(value)
        target = repository_target(self.source, value)
        if target is None:
            return value

        if not raw and target in self.chapter_ids:
            fragment = self.chapter_ids[target]
            if parts.fragment:
                fragment += "-" + unquote(parts.fragment)
            return urlunsplit(("", "", "", parts.query, fragment))

        relative = quote(target.relative_to(REPOSITORY_ROOT).as_posix(), safe="/")
        url = urlsplit((RAW_URL if raw else GITHUB_URL) + relative)
        return urlunsplit((url.scheme, url.netloc, url.path, parts.query, parts.fragment))


def build_page(directory: Path, output: Path) -> int:
    directory = directory.resolve()
    output = output.resolve()
    documents = {
        path.resolve(): path.read_text(encoding="utf-8")
        for path in sorted(directory.glob("*.md"))
    }
    if not documents:
        raise ValueError(f"No Markdown files found in {directory}")
    if output in documents:
        raise ValueError("The output path must not overwrite a Markdown source.")

    # Follow the index's reading order, then include any additional chapters.
    index = directory / "README.md"
    order = []
    if index in documents:
        order.append(index)
        for link in re.findall(r"\[[^\]]*\]\(([^)]+)\)", documents[index]):
            target = repository_target(index, link)
            if target in documents and target not in order:
                order.append(target)
    order.extend(path for path in documents if path not in order)

    chapter_ids = {path: "chapter-" + slugify(path.stem, "-") for path in order}
    if len(set(chapter_ids.values())) != len(order):
        raise ValueError("Markdown filenames produce duplicate chapter anchors.")

    navigation = []
    chapters = []
    for source in order:
        renderer = markdown.Markdown(
            extensions=["fenced_code", "tables", "toc", "sane_lists", TechnicalNotation()]
        )
        links = DocumentLinks(renderer, source, chapter_ids)
        renderer.treeprocessors.register(links, "document_links", 1)
        try:
            content = renderer.convert(documents[source])
        except ValueError as error:
            raise ValueError(f"{source.name}: {error}") from error
        chapter_id = html.escape(chapter_ids[source], quote=True)
        navigation.append(
            f'<a href="#{chapter_id}">{html.escape(links.title)}</a>'
        )
        chapters.append(f'<article id="{chapter_id}">\n{content}\n</article>')

    page = f"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Minic technical documentation</title>
<style>{STYLE}</style>
</head>
<body>
<nav aria-label="Chapters">
<strong>Minic technical documentation</strong>
{chr(10).join(navigation)}
</nav>
<main>
{chr(10).join(chapters)}
</main>
<script>
const MATHJAX_URL = {repr(MATHJAX_URL)};
const MERMAID_URL = {repr(MERMAID_URL)};
</script>
{RENDER_SCRIPT}
</body>
</html>
"""
    output.write_text(page, encoding="utf-8")
    return len(chapters)


def main():
    directory = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "-o", "--output", type=Path, default=directory / "index.html",
        help="Output HTML file (default: index.html beside this script).",
    )
    args = parser.parse_args()
    output = args.output.resolve()
    try:
        count = build_page(directory, output)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    print(f"Built {output} from {count} Markdown files.")


if __name__ == "__main__":
    main()
