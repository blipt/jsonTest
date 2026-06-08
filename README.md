# JSON Stream Viewer

Console C++20 application for viewing very large JSON files with a top-level array
shape:

```json
[{...},{...},{}]
```

The application opens the file as a binary stream and reads one top-level object
block at a time. It keeps only discovered object offsets plus the currently
displayed raw object string in memory, so it can be used with large files such as
200 MB JSON dumps.

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

- `PageDown`: load and display the next `{...}` object block
- `PageUp`: load and display the previous `{...}` object block
- `q` or `Esc`: quit

The displayed block is printed exactly as it appears in the source file, including
nested objects, arrays, whitespace, and escaped characters inside strings.
