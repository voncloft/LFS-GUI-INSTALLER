name=diffutils
echo "step:Compiling toolchain component $name"

autountar "$name"
cd $name*/

./configure --prefix=/usr   \
            --host=$LFS_TGT \
            gl_cv_func_strcasecmp_works=yes \
            --build=$(./build-aux/config.guess)

make
make DESTDIR=$LFS install
