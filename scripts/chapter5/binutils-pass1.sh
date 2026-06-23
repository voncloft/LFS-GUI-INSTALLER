set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
SCRIPT_PATH="$SCRIPT_DIR/$(basename -- "${BASH_SOURCE[0]}")"
if [ "$(id -un)" != "lfs" ]; then
  printf -v RELAUNCH_COMMAND 'cd -- %q && bash %q' "$SCRIPT_DIR" "$SCRIPT_PATH"
  exec su lfs -s /bin/bash -c "$RELAUNCH_COMMAND"
fi

source ../universal/default_modified.sh
source ../universal/cd_compile.sh

echo "step:Binutils-pass 1"

tar xvf binutils*.tar.xz

cd binutils*
mkdir -v build
cd build

../configure --prefix=$LFS/tools \
             --with-sysroot=$LFS \
             --target=$LFS_TGT   \
             --disable-nls       \
             --enable-gprofng=no \
             --disable-werror    \
             --enable-new-dtags  \
             --enable-default-hash-style=gnu

make
make install

source ../universal/cleanup.sh
