set -euo pipefail
echo "step:Installing prerequisites of host os"

export DEBIAN_FRONTEND=noninteractive
apt-get -y \
  -o Dpkg::Use-Pty=0 \
  -o Dpkg::Progress-Fancy=0 \
  -o APT::Color=0 \
  install binutils bison gawk gcc g++ m4 make texinfo yacc bash </dev/null
