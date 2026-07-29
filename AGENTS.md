# Agent Repository Guidance

## Index Scope

- CodeGraph and graphify intentionally index only the actively developed code
  under `engine/`, `hdRobot/`, and `headlessTraining/`.
- Stable `UsdRaySensor/` and `UsdRaySensorImaging/` code and temporary
  `python/` code are excluded from both indexes.

## FileMap

Use FileMap only when the task's owning domain or first source entry point is
unclear, or when the task genuinely crosses domains. If the prompt names a
relevant directory, file, or symbol, or CodeGraph resolves it directly, skip
FileMap.

- When needed, read the compact `docs/FILEMAP.md` router.
- Read only the relevant domain map selected under `docs/filemap/`; do not load
  every domain map by default.
- Treat the maps as navigation aids, then verify definitions and call paths
  against current source.
- Keep `docs/FILEMAP.md` and the affected domain map current when adding,
  moving, or substantially changing modules.

## graphify

This project has a knowledge graph at graphify-out/ with god nodes, community structure, and cross-file relationships.

When the user types `/graphify`, use the installed graphify skill or instructions before doing anything else.

Rules:
- For codebase questions, first run `graphify query "<question>"` when graphify-out/graph.json exists. Use `graphify path "<A>" "<B>"` for relationships and `graphify explain "<concept>"` for focused concepts. These return a scoped subgraph, usually much smaller than GRAPH_REPORT.md or raw grep output.
- Dirty graphify-out/ files are expected after hooks or incremental updates; dirty graph files are not a reason to skip graphify. Only skip graphify if the task is about stale or incorrect graph output, or the user explicitly says not to use it.
- If graphify-out/wiki/index.md exists, use it for broad navigation instead of raw source browsing.
- Read graphify-out/GRAPH_REPORT.md only for broad architecture review or when query/path/explain do not surface enough context.
- After modifying code, run `graphify update .` to keep the graph current (AST-only, no API cost).
