autountar() {
    if [ "$#" -ne 1 ]; then
        echo "usage: autountar <package-prefix>" >&2
        return 1
    fi

    local name="$1"
    local archives=()

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
        return 1
    fi

    tar -xf "${archives[0]}"
}
