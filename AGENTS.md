## graphify

This project has a graphify knowledge graph at graphify-out/.

Rules:

- Before answering architecture or codebase questions, read graphify-out/GRAPH_REPORT.md for god nodes and community structure
- After modifying code files in this session, run `python3 -c "from graphify.watch import _rebuild_code; from pathlib import Path; _rebuild_code(Path('.'))"` to keep the graph current，don't build code

## visual regression

Use render image diff as the default self-check workflow for functional changes:

- Capture baseline once: `bash install.sh baseline`
- After modifications: `bash install.sh selfcheck`
- If rendering behavior changes intentionally, refresh baseline after manual confirmation
