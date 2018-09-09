#!/usr/bin/env bash

set -e                     # fail if any command has a non-zero exit status
set -u                     # fail if any undefined variable is referenced
set -o pipefail            # propagate failure exit status through a pipeline
shopt -s globstar nullglob # enable recursive and null globbing

exe_name="tunnel-runner"
build_dir="./build"

cc=clang
source_files=("tunnel_runner.c")
cflags=("-std=c99" "-Wall" "-Wextra" "-Wshadow" "-Wswitch-enum" "-Wno-missing-braces")
debug_flags=("-g" "-Og" "-Werror")
release_flags=("-O2" "-Os" "-DTR_LOGLEVEL_DEBUG")
# shellcheck disable=SC2207
ldflags=("-lm" $(sdl2-config --cflags --libs))

build_release() {
    release_dir="${build_dir}/release"
    release_path="${release_dir}/${exe_name}"
    mkdir -p "$release_dir"
    (
        set -x;
        "$cc" "${cflags[@]}" "${release_flags[@]}" "${source_files[@]}" -o "$release_path" "${ldflags[@]}"
    )
}

build_debug() {
    debug_dir="${build_dir}/debug"
    debug_path="${debug_dir}/${exe_name}"
    mkdir -p "$debug_dir"
    (
        set -x;
        "$cc" "${cflags[@]}" "${debug_flags[@]}" "${source_files[@]}" -o "$debug_path" "${ldflags[@]}"
    )
}

usage() {
    echo "build.sh - Build ${exe_name}"
    echo " "
    echo "build.sh [options]"
    echo " "
    echo "options:"
    echo "-h, --help                show help"
    echo "-r, --release-only        build only release executable"
    echo "-d, --debug-only          build only debug executable"
}

release_only=false
debug_only=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        -h|--help)
            usage
            exit 0
            ;;
        -r|--release-only)
            release_only=true
            shift
            ;;
        -d|--debug-only)
            debug_only=true
            shift
            ;;
        *)
            break
            ;;
    esac
done

if [ "$debug_only" = false ]; then
    build_release
fi
if [ "$release_only" = false ]; then
    build_debug
fi

exit 0
