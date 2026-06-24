name=gzip
echo "step:Compiling toolchain component $name"

autountar "$name"
cd $name*/

./configure --prefix=/usr --host=$LFS_TGT
make
make DESTDIR=$LFS install
