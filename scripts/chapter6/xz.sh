name=xz
echo "step:Compiling toolchain component $name"

sh autountar "$name"
cd $name*/

./configure --prefix=/usr                     \
            --host=$LFS_TGT                   \
            --build=$(build-aux/config.guess) \
            --disable-static                  \
            --docdir=/usr/share/doc/xz-5.8.3

make
make DESTDIR=$LFS install
rm -v $LFS/usr/lib/liblzma.la
