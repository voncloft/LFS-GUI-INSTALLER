name=gcc
echo "step:Compiling toolchain component $name"

sh autountar "$name"
cd $name*/

tar -xf ../mpfr-$mpfr_version.tar.xz
mv -v mpfr-$mpfr_version mpfr
tar -xf ../gmp-$gmp_version.tar.xz
mv -v gmp-$gmp_version gmp
tar -xf ../mpc-$mpc_version.tar.xz
mv -v mpc-$mpc_version mpc

sed -e '/m64=/s/lib64/lib/' \
    -e '/m32=/s/m32=.*/m32=..\/lib32$(call if_multiarch,:i386-linux-gnu)/' \
    -i.orig gcc/config/i386/t-linux64

sed '/STACK_REALIGN_DEFAULT/s/0/(!TARGET_64BIT \&\& TARGET_SSE)/' \
      -i gcc/config/i386/i386.h

mkdir -v build
cd       build

../configure                     \
    --build=$(../config.guess)   \
    --host=$LFS_TGT              \
    --target=$LFS_TGT            \
    --prefix=/usr                \
    --with-build-sysroot=$LFS    \
    --enable-default-pie         \
    --enable-default-ssp         \
    --disable-nls                \
    --enable-multilib            \
    --with-multilib-list=m64,m32 \
    --disable-libatomic          \
    --disable-libgomp            \
    --disable-libquadmath        \
    --disable-libsanitizer       \
    --disable-libssp             \
    --disable-libvtv             \
    --enable-languages=c,c++     \
    LDFLAGS_FOR_TARGET=-L$PWD/$LFS_TGT/libgcc \
    target_configargs=gcc_cv_target_thread_file=posix

make
make DESTDIR=$LFS install
ln -sv gcc $LFS/usr/bin/cc
