# Minic pytest entry point

You probably want this

```
pytest -q -s Test/test.py
```

`-gameinfo_test` checks optional root-search scores, UCI history replay and branch
invalidation, XBoard rewind, new-game resets, and booming/moobing thresholds.
Scores belong to searched roots and use the side-to-move perspective; unsearched
positions remain unscored. Only completed search results are recorded, after SMP
result selection, separately from the search stack's static evaluations.

Booming/moobing compares scores two and four plies before the current root.
Missing or mate-range scores disable the heuristic; a rise or fall of at least
80 points triggers it. Rebuilding a history preserves scores only along its
unchanged prefix, including an identical initial FEN.
