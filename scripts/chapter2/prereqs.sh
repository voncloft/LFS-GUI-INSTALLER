set -euo pipefail
echo "step:Installing prerequisites of host os"

sudo DEBIAN_FRONTEND=noninteractive apt install -y binutils bison gawk gcc g++ m4 make texinfo yacc bash </dev/null
