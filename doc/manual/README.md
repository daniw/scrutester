# ScruTester user manual

LaTeX project for the user manual (English and German), shares
`../layout/layout.sty` with the technical documentation.

* `make` builds both, `make en` / `make de` one language. PDFs land in `obj/`.
* `src/en/` and `src/de/` have the same file structure; change both together.
* `manual.sty`: manual-only macros (buttons, display texts, safety boxes).
* `TODO.md`: open points, missing information.
