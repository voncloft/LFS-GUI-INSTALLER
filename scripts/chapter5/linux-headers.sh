source ../universal/versions.sh

name=linux
echo "step:Compiling toolchain component linux API headers"

sh autountar $name
cd $name*/

make mrproper
make headers
find usr/include -type f ! -name '*.h' -delete
cp -rv usr/include $LFS/usr
