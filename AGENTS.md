## graphify

This project has a graphify knowledge graph at graphify-out/.

Rules:

- Before answering architecture or codebase questions, read graphify-out/GRAPH_REPORT.md for god nodes and community structure
- After reading the graph report, read docs/FILEMAP.md to choose the first files and symbols to inspect
- Keep docs/FILEMAP.md current when adding, moving, or substantially changing modules
- After modifying code files in this session, run `python3 -c "from graphify.watch import _rebuild_code; from pathlib import Path; _rebuild_code(Path('.'))"` to keep the graph current
- Prefer splitting work into multiple subtasks and delegating them to subagents to reduce per-session context usage
