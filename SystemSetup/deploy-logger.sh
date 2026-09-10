#!/bin/bash
# Installs the built logger as the PRODUCTION copy in ~/bin, separately from the git working tree.
#
# The two copies exist because the configuration and the source now share a file. KnurLogger.ini
# carries the calibration offsets AND the DS18B20 bindings, the bindings can only be made at the
# car, and the tar-over-ssh build loop overwrites everything under ~/KnurLogger. A single copy
# would mean one sync from the workstation silently discards a trip to the car.
#
#   ~/KnurLogger/build/KnurLogger.ini   the DEV copy - git-tracked, a template, freely overwritten
#   ~/bin/KnurLogger.ini                the PRODUCTION copy - real values, NEVER overwritten here
#
# So this script always replaces the binary and only ever creates the .ini, never updates it. To
# put the field values back under version control, copy the production .ini into the repo on the
# workstation and commit it - that is a deliberate act, which is the point.
#
# Pre-flight by default, like every script in this directory. Run it, read the + lines, run it
# again with --execute.

set -u

PATH="$PATH:/usr/sbin:/sbin"

SOURCE_DIR="$HOME/KnurLogger/build"
TARGET_DIR="$HOME/bin"
IS_EXECUTE=0

for argument in "$@"; do
    case "$argument" in
        --execute) IS_EXECUTE=1 ;;
        *) echo "Unknown argument: $argument"; echo "Usage: $0 [--execute]"; exit 2 ;;
    esac
done

if [ "$IS_EXECUTE" -eq 0 ]; then
    echo "PRE-FLIGHT ONLY. Re-run with --execute to apply."
    echo
fi

if [ ! -x "$SOURCE_DIR/KnurLogger" ]; then
    echo "FAIL: no built binary at $SOURCE_DIR/KnurLogger. Build it first:"
    echo "        cmake --build ~/KnurLogger/build -j4"
    exit 1
fi

echo "+ mkdir -p $TARGET_DIR"
[ "$IS_EXECUTE" -eq 1 ] && mkdir -p "$TARGET_DIR"

# Written through a temporary and renamed rather than copied over in place, so that replacing the
# binary under a running logger swaps the directory entry instead of truncating the file it is
# executing.
echo "+ install binary: $SOURCE_DIR/KnurLogger -> $TARGET_DIR/KnurLogger"
if [ "$IS_EXECUTE" -eq 1 ]; then
    cp "$SOURCE_DIR/KnurLogger" "$TARGET_DIR/KnurLogger.new" || exit 1
    chmod 755 "$TARGET_DIR/KnurLogger.new"
    mv -f "$TARGET_DIR/KnurLogger.new" "$TARGET_DIR/KnurLogger" || exit 1
fi

if [ -f "$TARGET_DIR/KnurLogger.ini" ]; then
    echo "  KEEPING $TARGET_DIR/KnurLogger.ini - it holds the bindings and the offsets."
    echo "  Its current bindings:"
    sed -n 's/^\(temp[0-3]RomId=.*\)$/    \1/p' "$TARGET_DIR/KnurLogger.ini"
    echo "  Diff against the repo template (informational; nothing here applies it):"
    diff "$SOURCE_DIR/KnurLogger.ini" "$TARGET_DIR/KnurLogger.ini" | sed 's/^/    /' \
        || true
else
    echo "+ seed config from the template: $SOURCE_DIR/KnurLogger.ini -> $TARGET_DIR/KnurLogger.ini"
    echo "  (first install only - it is never overwritten again)"
    [ "$IS_EXECUTE" -eq 1 ] && cp "$SOURCE_DIR/KnurLogger.ini" "$TARGET_DIR/KnurLogger.ini"
fi

echo
if [ "$IS_EXECUTE" -eq 1 ]; then
    echo "Done. The production logger is $TARGET_DIR/KnurLogger, reading $TARGET_DIR/KnurLogger.ini."
    echo "KnurLogger.service must point at that path, not at the build tree."
else
    echo "Nothing was changed. Re-run with --execute."
fi
