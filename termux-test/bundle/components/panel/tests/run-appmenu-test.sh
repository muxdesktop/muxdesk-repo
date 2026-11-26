#!/bin/sh
# End-to-end global-menu test: needs a private session bus.
# Usage: tests/run-appmenu-test.sh [path-to-test-appmenu-binary]
set -e
BIN="${1:-build/test-appmenu}"
DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
exec dbus-run-session -- "$DIR/$BIN" "$DIR/tests/fake-menu-app.py"
