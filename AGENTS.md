# Agent Repository Guidance

## Index Scope

- CodeGraph and graphify intentionally index only the actively developed code
  under `engine/`, `hdRobot/`, and `headlessTraining/`.
- Stable `UsdRaySensor/` and `UsdRaySensorImaging/` code and temporary
  `python/` code are excluded from both indexes.

## Graphify

Use Graphify for architecture, cross-module relationships, and dependency-path
questions. Skip it for localized file or symbol work.

- Read `graphify-out/GRAPH_SUMMARY.md` as the default graph entry point.
- Do not read `graphify-out/GRAPH_REPORT.md` in full by default. Search it by
  task term or community ID and read only matching sections when deeper graph
  context is needed.
- Never load `graphify-out/graph.json` wholesale. Use bounded, task-specific
  traversal for relationship or path questions.
- After modifying indexed code, run `bash install.sh graphify_index` to rebuild
  the graph and its compact summary.

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
