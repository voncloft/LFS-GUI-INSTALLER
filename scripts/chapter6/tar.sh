source ../universal/versions.sh

name=tar
echo "step:Compiling toolchain component $name"

sh autountar "$name"
cd $name*/

./configure --prefix=/usr   \
            --host=$LFS_TGT \
            --build=$(build-aux/config.guess)

make
make DESTDIR=$LFS install
