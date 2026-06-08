# JSON Stream Viewer
Console C++20 application for viewing very large JSON files with a top-level array shape:

```json
[{...},{...},{}]
```
## Build

```bash
cmake -S . -B build
cmake --build build
```

## Run
```bash
./build/json_stream_viewer /path/to/file.json
```

Controls:
- `PageUp`, `ArrowUp` and `PageDown`, `ArrowDown`: scroll the current block up and down
- `q` - quit
