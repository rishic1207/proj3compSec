#!/bin/bash
# Build the Canvas submission tarball: project3.<pid1>.<pid2>.tar.gz
# Usage (from the folder holding obj1..obj9 and the .txt files):
#   ./package.sh pid1 pid2
# Copies source files (and a seeds/ folder, if present) from objN/ into
# srcFilesN/, skipping binaries, logs, trial folders, and generated output.
set -e
[ $# -eq 2 ] || { echo "usage: $0 pid1 pid2" >&2; exit 1; }
for n in 1 3 4 5 6 7 8 9; do
    [ -d "obj$n" ] || { echo "note: obj$n/ not found, skipping srcFiles$n"; continue; }
    rm -rf "srcFiles$n" && mkdir "srcFiles$n"
    for f in obj$n/*; do
        case "$(basename "$f")" in
            Makefile|*.c|*.h|*.py|*.sh) cp "$f" "srcFiles$n/" ;;
        esac
    done
    if [ -d "obj$n/seeds" ]; then
        cp -r "obj$n/seeds" "srcFiles$n/"
        echo "included obj$n/seeds"
    fi
done
missing=0
for f in obj1.txt obj2.txt obj3.txt obj4.txt obj5.txt obj6.txt obj7.txt obj8.txt obj9.txt ai.txt; do
    [ -f "$f" ] || { echo "MISSING: $f"; missing=1; }
done
[ $missing -eq 0 ] || echo "warning: tarball will be missing the files above"
out="project3.$1.$2.tar.gz"
tar -zcf "$out" $(ls -d obj[1-9].txt srcFiles[13456789] ai.txt 2>/dev/null)
echo "Created $out:"
tar -tzf "$out"
