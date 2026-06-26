name=bison
echo "step:Installing $name"

sh autountar "$name"
cd $name*/

./configure --prefix=/usr \
            --docdir=/usr/share/doc/bison-$bison_version
make
make install
