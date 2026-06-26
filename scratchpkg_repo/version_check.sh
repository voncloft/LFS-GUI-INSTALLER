#!/bin/bash
set -e

usage() {
    echo "Usage: $0 [-m|-n|-s|-h]"
    echo "  -m   Show missing packages"
    echo "  -n   Show packages with newer versions"
    echo "  -s   Generate sed script (with folder/spkgbuild, do not apply)"
    echo "  -h   Show this help"
}

show_missing=false
show_new=false
generate_sed=false

while getopts "mnsHh" opt; do
    case "$opt" in
        m) show_missing=true ;;
        n) show_new=true ;;
        s) generate_sed=true ;;
        h|H) usage; exit 0 ;;
        *) usage; exit 1 ;;
    esac
done

url="https://www.linuxfromscratch.org/~thomas/multilib-m32/wget-list-sysv"
dest="wget-list-sysv"
output="package_versions.txt"
sedfile="version_fix.sh"

echo "Downloading $url ..."
wget -O "$dest" "$url"
sed -i '/\.patch$/d' wget-list-sysv
sed -i '/html/d' wget-list-sysv
sed -i -e 's/tcl/tcl-/g' wget-list-sysv
sed -i -e 's/expect/expect-/g' wget-list-sysv
> "$output"
> "$sedfile"

# Parse wget list
while read -r fileurl; do
    [[ -z "$fileurl" || "$fileurl" =~ ^# ]] && continue
    filename="${fileurl##*/}"

    # Remove non-version suffixes
    base="${filename%%.tar.*}"
    base="${base%%.zip}"
    base="${base%-src}"
    base="${base%-html}"
    base="${base%-docs}"
    base="${base%-1.patch}"

    pkg="${base%-*}"
    ver="${base##*-}"

    echo "${pkg}=${ver}" >> "$output"
done < "$dest"

# Manual name mapping
declare -A name_map=(
    ["wheel"]="python-wheel"
    ["flit"]="python-flit"
    ["packaging"]="python-packaging"
    ["setuptools"]="python-setuptools"
    ["xml-parser"]="perl-xml-parser"
    ["xmlparser"]="perl-xml-parser"
    ["tcl8"]="tcl"
    ["tcl8."]="tcl"
    ["expect5"]="expect"
    ["expect."]="expect"
    ["ncurses6"]="ncurses"
    ["ncurses."]="ncurses"
    ["pkgconf"]="pkg-config"
)

mapfile -t local_dirs < <(find . -mindepth 1 -maxdepth 1 -type d -printf "%P\n" | tr 'A-Z' 'a-z')

while IFS="=" read -r pkg ver; do
    original_pkg="$pkg"
    pkg_lc=$(echo "$pkg" | tr 'A-Z' 'a-z')

    # Apply mapping
    for key in "${!name_map[@]}"; do
        if [[ "$pkg_lc" =~ ^$key ]]; then
            pkg="${name_map[$key]}"
            pkg_lc=$(echo "$pkg" | tr 'A-Z' 'a-z')
            break
        fi
    done

    # Match folder
    dir=""
    for d in "${local_dirs[@]}"; do
        if [[ "$pkg_lc" == "$d" ]]; then
            dir="$d"
            break
        fi
    done

    if [[ -z "$dir" ]]; then
        [[ "$show_missing" == true ]] && echo "$original_pkg: missing"
        continue
    fi

    spkgbuild="./$dir/spkgbuild"
    if [[ ! -f "$spkgbuild" ]]; then
        [[ "$show_missing" == true ]] && echo "$dir: spkgbuild not found"
        continue
    fi

    local_ver=$(grep -E '^version=' "$spkgbuild" | cut -d= -f2)

    if [[ "$ver" != "$local_ver" ]]; then
        [[ "$show_new" == true ]] && echo "$dir: newer available ($local_ver → $ver)"
        [[ "$generate_sed" == true ]] && echo "sed -i -e s/^version=.*/version=$ver/g $dir/spkgbuild" >> "$sedfile"
    fi
done < "$output"

[[ "$generate_sed" == true ]] && echo "Sed script written to $sedfile"
echo "Done."
