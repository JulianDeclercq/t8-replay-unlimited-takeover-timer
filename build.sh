#!/bin/sh
set -e
# ponytail: pause so a double-clicked window stays open; skipped when not a tty.
[ -t 0 ] && trap 'echo; echo "Press Enter to close..."; read _' EXIT
cd "$(dirname "$0")"
cmake -B build -A x64
cmake --build build --config Release
