name=findutils
echo "step:Compiling toolchain component $name"

sh autountar "$name"
cd $name*/

./configure --prefix=/usr                   \
            --localstatedir=/var/lib/locate \
            --host=$LFS_TGT                 \
            --build=$(build-aux/config.guess)

make
make DESTDIR=$LFS install
