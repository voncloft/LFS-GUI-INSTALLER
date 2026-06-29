set -euo pipefail
echo "step:Installing prerequisites of host os"

export DEBIAN_FRONTEND=noninteractive
apt-get -y \
  -o Dpkg::Use-Pty=0 \
  -o Dpkg::Progress-Fancy=0 \
  -o APT::Color=0 \
  install \
  bash \
  binutils \
  bison \
  ca-certificates \
  docbook-xml \
  docbook-xsl \
  docbook-xsl-ns \
  g++ \
  gawk \
  gcc \
  git \
  libxml2-utils \
  m4 \
  make \
  python3 \
  sudo \
  texinfo \
  wget \
  xsltproc \
  yacc </dev/null
