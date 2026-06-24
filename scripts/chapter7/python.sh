name=python
echo "step:Installing $name"

autountar "$name"
cd $name*/

./configure --prefix=/usr       \
            --enable-shared     \
            --without-ensurepip \
            --without-static-libpython

make

make install
