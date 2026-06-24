name=m4
echo "step:Compiling toolchain compenent $name"

autountar $name
cd $name*/

./configure --prefix=/usr   \
            --host=$LFS_TGT \
            --build=$(build-aux/config.guess)

make
make DESTDIR=$LFS install
