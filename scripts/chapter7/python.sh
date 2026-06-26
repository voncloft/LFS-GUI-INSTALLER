source ../universal/versions.sh

name=python
echo "step:Installing $name"

sh autountar "$name"
cd $name*/

./configure --prefix=/usr       \
            --enable-shared     \
            --without-ensurepip \
            --without-static-libpython

make

make install
