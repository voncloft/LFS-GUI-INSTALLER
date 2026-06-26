source ../universal/versions.sh

name=gawk
echo "step:Compiling toolchain component $name"

sh autountar "$name"
cd $name*/

sed -i 's/extras//' Makefile.in
./configure --prefix=/usr   \
            --host=$LFS_TGT \
            --build=$(build-aux/config.guess)

make
make DESTDIR=$LFS install
