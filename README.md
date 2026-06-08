# JSON Stream Viewer
Console C++20 application for viewing very large JSON files with a top-level array shape:

```json
[{...},{...},{}]
```

## Features

- **Scrollable interface**: When using ncurses (Linux/Unix), the application provides a scrollable text view similar to `less` or `more` commands
- **Mouse wheel support**: Scroll through content using your mouse wheel (in terminals that support it)
- **Keyboard navigation**: Use arrow keys, Page Up/Down for navigation
- **Memory efficient**: Streams large JSON files without loading everything into memory

## Build

### Prerequisites

**Linux/Unix:**
```bash
# Install ncurses development library
sudo apt-get install libncurses5-dev  # Debian/Ubuntu
sudo yum install ncurses-devel        # RHEL/CentOS
sudo pacman -S ncurses                # Arch Linux
```

**Windows:**
- Native Windows console support (no additional dependencies)

### Build Commands

```bash
cmake -S . -B build
cmake --build build
```

## Run

```bash
./build/json_stream_viewer /path/to/file.json
```

## Controls

### With ncurses (Linux/Unix with curses support):
- `↑` / `↓` (Arrow Up/Down): Scroll one line at a time
- `PageUp` / `PageDown`: Scroll one page at a time
- **Mouse wheel**: Scroll up/down (in supported terminals)
- `q` or `Q` or `ESC`: Quit

### Without ncurses (Windows or fallback mode):
- `PageUp`, `ArrowUp`: Go to previous JSON object
- `PageDown`, `ArrowDown`: Go to next JSON object
- `q`: Quit

## How It Works

The application uses the **ncurses library** when available to provide a proper terminal UI with:
- Scrollable text buffer (like `less` command)
- Mouse wheel support for scrolling
- Status bar showing scroll position
- Smooth line-by-line and page-by-page navigation

When ncurses is not available (e.g., on Windows without PDCurses), the application falls back to the original behavior of navigating between JSON objects.
