echo "step:Linux API headers"

autountar linux
cd linux*/

make mrproper
make headers
find usr/include -type f ! -name '*.h' -delete
cp -rv usr/include $LFS/usr
