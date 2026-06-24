#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 1 ]; then
    echo "usage: autountar <package-prefix>" >&2
    exit 1
fi

name="$1"
archives=()

shopt -s nullglob
archives=(
    "${name}"*.tar.xz
    "${name}"*.tar.gz
    "${name}"*.tar.bz2
    "${name}"*.tar.zst
    "${name}"*.tgz
    "${name}"*.tar
)
shopt -u nullglob

if [ "${#archives[@]}" -eq 0 ]; then
    echo "autountar: no archive found for ${name}" >&2
    exit 1
fi

tar -xf "${archives[0]}"
