set -euo pipefail
source ../universal/cd_compile.sh
source  ../universal/default_modified.sh

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
