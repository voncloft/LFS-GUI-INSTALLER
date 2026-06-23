set -euo pipefail
echo "step:Installing prerequisites of host os"

sudo apt install -y binutils bison gawk gcc g++ m4 make texinfo yacc bash
