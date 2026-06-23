echo "step:Linux API headers"

tar xvf linux*.tar.xz
cd linux*/

make mrproper
make headers
find usr/include -type f ! -name '*.h' -delete
cp -rv usr/include $LFS/usr
