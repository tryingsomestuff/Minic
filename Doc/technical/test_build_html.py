"""Run with: python -m unittest discover -s Doc/technical -p 'test_*.py'."""

from html.parser import HTMLParser
from pathlib import Path
import tempfile
import unittest

import markdown

from build_html import build_page, TechnicalNotation


class MathText(HTMLParser):
    def __init__(self):
        super().__init__()
        self.text = []
        self.depth = 0

    def handle_starttag(self, tag, attributes):
        if self.depth or "math" in dict(attributes).get("class", "").split():
            self.depth += 1

    def handle_endtag(self, tag):
        if self.depth:
            self.depth -= 1

    def handle_data(self, text):
        if self.depth:
            self.text.append(text)


class NotationTests(unittest.TestCase):
    def render(self, source):
        return markdown.Markdown(
            extensions=["fenced_code", "tables", TechnicalNotation()]
        ).convert(source)

    def test_display_math_preserves_tex(self):
        tex = r"\begin{aligned}x_1 &= \alpha < \beta \\ x_2 &= 2\end{aligned}"
        rendered = self.render("$$\n" + tex + "\n$$")
        parsed = MathText()
        parsed.feed(rendered)
        self.assertEqual("".join(parsed.text), "$$\n" + tex + "\n$$")
        self.assertNotIn("<em>", rendered)

    def test_inline_math_preserves_tex_in_tables(self):
        rendered = self.render("| Value |\n|---|\n| $A_{c,j} + \\alpha$ |")
        parsed = MathText()
        parsed.feed(rendered)
        self.assertEqual("".join(parsed.text), r"\(A_{c,j} + \alpha\)")

    def test_code_is_not_math(self):
        rendered = self.render(
            "```text\n$$\nx_1\n$$\n```\n\n"
            "`$x_1$`\n\n    $$\n    x_2\n    $$"
        )
        self.assertNotIn('class="math', rendered)
        self.assertEqual(rendered.count("<pre>"), 2)

    def test_escaped_dollars_are_not_math(self):
        self.assertNotIn('class="math', self.render(r"\$x_1\$"))

    def test_mermaid_source_survives_markdown(self):
        rendered = self.render('```mermaid\nflowchart LR\nA["Root"] --> B["Search"]\n```')
        self.assertIn('class="language-mermaid"', rendered)
        self.assertIn("A[", rendered)
        self.assertIn("--&gt;", rendered)

    def test_invalid_display_math_is_explicit(self):
        for source in ("$$\nx_1", "$$\n\n$$"):
            with self.subTest(source=source), self.assertRaises(ValueError):
                self.render(source)

    def test_standalone_page_has_renderers_and_readable_source(self):
        with tempfile.TemporaryDirectory(prefix="minic-html-test-") as temporary:
            directory = Path(temporary)
            (directory / "README.md").write_text(
                "# Example\n\n$x_1$\n\n```mermaid\nflowchart LR\nA --> B\n```\n",
                encoding="utf-8",
            )
            output = directory / "page.html"
            self.assertEqual(build_page(directory, output), 1)
            page = output.read_text(encoding="utf-8")
            self.assertIn("mathjax@3.2.2", page)
            self.assertIn("mermaid@11.12.0", page)
            self.assertIn("LaTeX source remains readable.", page)
            self.assertIn(r"\(x_1\)", page)
            self.assertIn('class="language-mermaid"', page)


if __name__ == "__main__":
    unittest.main()
