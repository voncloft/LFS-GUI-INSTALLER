name=bash
echo "step:Compiling toolchain component $name"

sh autountar "$name"
cd $name*/

./configure --prefix=/usr                      \
            --build=$(sh support/config.guess) \
            --host=$LFS_TGT                    \
            --without-bash-malloc              \
            --docdir=/usr/share/doc/bash-$bash_version

make
make DESTDIR=$LFS install
ln -sv bash $LFS/bin/sh
