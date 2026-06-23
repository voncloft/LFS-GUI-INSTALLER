set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
SCRIPT_PATH="$SCRIPT_DIR/$(basename -- "${BASH_SOURCE[0]}")"
if [ "$(id -un)" != "lfs" ]; then
  printf -v RELAUNCH_COMMAND 'cd -- %q && bash %q' "$SCRIPT_DIR" "$SCRIPT_PATH"
  exec su lfs -s /bin/bash -c "$RELAUNCH_COMMAND"
fi

source ../universal/default_modified.sh
source ../universal/cd_compile.sh

echo "gcc:Binutils-pass 1"

tar xvf gcc*.tar.xz
cd gcc*
tar -xf ../mpfr-4.2.2.tar.xz
mv -v mpfr-4.2.2 mpfr
tar -xf ../gmp-6.3.0.tar.xz
mv -v gmp-6.3.0 gmp
tar -xf ../mpc-1.4.1.tar.xz
mv -v mpc-1.4.1 mpc

sed -e '/m64=/s/lib64/lib/' \
    -e '/m32=/s/m32=.*/m32=..\/lib32$(call if_multiarch,:i386-linux-gnu)/' \
    -i.orig gcc/config/i386/t-linux64

sed '/STACK_REALIGN_DEFAULT/s/0/(!TARGET_64BIT \&\& TARGET_SSE)/' \
      -i gcc/config/i386/i386.h

mkdir -v build
cd       build

../configure                     \
    --target=$LFS_TGT            \
    --prefix=$LFS/tools          \
    --with-glibc-version=2.43    \
    --with-sysroot=$LFS          \
    --with-newlib                \
    --without-headers            \
    --enable-default-pie         \
    --enable-default-ssp         \
    --enable-initfini-array      \
    --disable-nls                \
    --disable-shared             \
    --enable-multilib            \
    --with-multilib-list=m64,m32 \
    --disable-decimal-float      \
    --disable-threads            \
    --disable-libatomic          \
    --disable-libgomp            \
    --disable-libquadmath        \
    --disable-libssp             \
    --disable-libvtv             \
    --disable-libstdcxx          \
    --enable-languages=c,c++

make 
make install

cat ../gcc/{limitx,glimits,limity}.h  > \
  $($LFS_TGT-gcc -print-file-name=include)/limits.h

source ../universal/cleanup.sh
