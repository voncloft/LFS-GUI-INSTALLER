name=bison
echo "step:Installing $name"

sh autountar "$name"
cd $name*/

./configure --prefix=/usr \
            --docdir=/usr/share/doc/bison-3.8.2
make
make install
