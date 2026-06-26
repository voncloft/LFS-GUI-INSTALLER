#!/usr/bin/env bash
set -euo pipefail
unset BASH_ENV ENV
STAGED_SCRIPTS='/home/kubuntu/Desktop/LFS-GUI-INSTALLER/run-20260626-014336/generated-artifacts/scripts'
STAGED_FILES='/home/kubuntu/Desktop/LFS-GUI-INSTALLER/run-20260626-014336/generated-artifacts/files'
STAGED_ROOT='/home/kubuntu/Desktop/LFS-GUI-INSTALLER/run-20260626-014336/generated-artifacts'
TARGET_SCRIPTS='/home/kubuntu/Desktop/LFS-GUI-INSTALLER/scripts'
TARGET_FILES='/home/kubuntu/Desktop/LFS-GUI-INSTALLER/files'
install -d "$TARGET_SCRIPTS" "$TARGET_FILES"
install -m 755 "$STAGED_SCRIPTS/final_setup.sh" "$TARGET_SCRIPTS/final_setup.sh"
install -m 755 "$STAGED_SCRIPTS/partition.sh" "$TARGET_SCRIPTS/partition.sh"
install -m 755 "$STAGED_SCRIPTS/mount.sh" "$TARGET_SCRIPTS/mount.sh"
install -m 644 "$STAGED_FILES/hostname" "$TARGET_FILES/hostname"
install -m 644 "$STAGED_FILES/clock" "$TARGET_FILES/clock"
install -m 644 "$STAGED_FILES/fstab" "$TARGET_FILES/fstab"
set -euo pipefail
unset BASH_ENV ENV
PROJECT_ROOT='/home/kubuntu/Desktop/LFS-GUI-INSTALLER/run-20260626-014336/generated-artifacts'
export PROJECT_ROOT
echo '__SCRIPT_BEGIN__:universal/versions.sh'
set -euo pipefail
set -x
acl_version=2.3.2
attr_version=2.5.2
autoconf_version=2.72
automake_version=1.18.1
bash_version=5.3
bc_version=7.0.3
binutils_version=2.46.0
bison_version=3.8.2
bzip2_version=1.0.8
coreutils_version=9.10
dbus_version=1.16.2
dejagnu_version=1.6.3
diffutils_version=3.12
e2fsprogs_version=1.47.3
elfutils_version=0.194
expat_version=2.7.4
expect_version=5.45.4
file_version=5.46
findutils_version=4.10.0
flex_version=2.6.4
flit_core_version=3.12.0
gawk_version=5.3.2
gcc_version=15.2.0
gdbm_version=1.26
gettext_version=1.0
glibc_version=2.43
gmp_version=6.3.0
gperf_version=3.3
grep_version=3.12
groff_version=1.23.0
grub_version=2.14
gzip_version=1.14
iana_etc_version=20260202
inetutils_version=2.7
intltool_version=0.51.0
iproute2_version=6.18.0
isl_version=0.27
jinja2_version=3.1.6
kbd_version=2.9.0
kmod_version=34.2
less_version=692
libcap_version=2.77
libffi_version=3.5.2
libpipeline_version=1.5.8
libtool_version=2.5.4
libxcrypt_version=4.5.2
linux_version=6.18.10
lz4_version=1.10.0
m4_version=1.4.21
make_version=4.4.1
man_db_version=2.13.1
man_pages_version=6.17
markupsafe_version=3.0.3
meson_version=1.10.1
mpc_version=1.3.1
mpfr_version=4.2.2
ncurses_version=6.6
ninja_version=1.13.2
openssl_version=3.6.1
packaging_version=26.0
patch_version=2.8
pcre2_version=10.47
perl_version=5.42.0
pkgconf_version=2.5.1
procps_version=4.0.6
psmisc_version=23.7
python_version=3.14.3
python_documentation_version=3.14.3
readline_version=8.3
sed_version=4.9
setuptools_version=82.0.0
shadow_version=4.19.3
sqlite_version=3510200
sqlite_documentation_version=3510200
systemd_version=259.1
systemd_man_pages_version=259.1
tar_version=1.35
tcl_version=8.6.17
tcl_documentation_version=8.6.17
texinfo_version=7.2
time_zone_data_version=2025c
util_linux_version=2.41.3
vim_version=9.2.0078
wheel_version=0.46.3
xml_parser_version=2.47
xz_utils_version=5.8.2
zlib_version=1.3.2
zstd_version=1.5.7

set +x
echo '__SCRIPT_DONE__:universal/versions.sh'
echo '__SCRIPT_BEGIN__:chapter2/prereqs.sh'
set -euo pipefail
set -x
set -euo pipefail
echo "step:Installing prerequisites of host os"

export DEBIAN_FRONTEND=noninteractive
apt-get -y \
  -o Dpkg::Use-Pty=0 \
  -o Dpkg::Progress-Fancy=0 \
  -o APT::Color=0 \
  install binutils bison gawk gcc g++ m4 make texinfo yacc bash </dev/null

set +x
echo '__SCRIPT_DONE__:chapter2/prereqs.sh'
echo '__SCRIPT_BEGIN__:partition.sh'
set -euo pipefail
set -x
#!/usr/bin/env bash

set -euo pipefail

TARGET_DRIVE='/dev/sda'

echo "step:Setting up partitions"
parted --script "$TARGET_DRIVE" mklabel gpt
parted --script "$TARGET_DRIVE" mkpart primary ext4 1MiB 2049MiB
parted --script "$TARGET_DRIVE" mkpart primary ext4 2049MiB 23553MiB
partprobe "$TARGET_DRIVE"

# /dev/sda1 -> /boot (ext4, 2.0 GiB)
mkfs.ext4 -F '/dev/sda1'
# /dev/sda2 -> / (ext4, 21.0 GiB)
mkfs.ext4 -F '/dev/sda2'

set +x
echo '__SCRIPT_DONE__:partition.sh'
echo '__SCRIPT_BEGIN__:mount.sh'
set -euo pipefail
set -x
#!/usr/bin/env bash

set -euo pipefail

echo "step:Mounting filesystems"
mkdir -p '/mnt/lfs'
mount '/dev/sda2' '/mnt/lfs'
mkdir -p '/mnt/lfs/boot'
mount '/dev/sda1' '/mnt/lfs/boot'

set +x
echo '__SCRIPT_DONE__:mount.sh'
echo '__SCRIPT_BEGIN__:chapter2/change_permissions.sh'
set -euo pipefail
set -x
set -euo pipefail
export LFS=/mnt/lfs
echo "step:Changing permissions of root drive"
chown root:root $LFS
chmod 755 $LFS

set +x
echo '__SCRIPT_DONE__:chapter2/change_permissions.sh'
echo '__SCRIPT_BEGIN__:chapter3/get_packages.sh'
set -euo pipefail
set -x
set -euo pipefail

echo "step:Getting Packages"
mkdir -v $LFS/sources
chmod -v a+wt $LFS/sources

#wget https://www.linuxfromscratch.org/mlfs/view/dev/wget-list-systemd
#wget --input-file=wget-list-systemd --continue --directory-prefix=$LFS/sources

#pushd $LFS/sources
#  md5sum -c md5sums
#popd
#wget https://sourceware.org/pub/binutils/releases/binutils-2.46.0.tar.xz
#mv binutils*.tar.xz $LFS/sources


wget -c https://ftp.osuosl.org/pub/lfs/lfs-packages/lfs-packages-13.0.tar --directory-prefix=$LFS/sources
tar -xvf $LFS/sources/lfs-packages-13.0.tar -C $LFS/sources
mv $LFS/sources/*/* $LFS/sources
cp ../../tools/autountar.sh $LFS/sources/autountar
chmod +x $LFS/sources/autountar

git clone https://github.com/voncloft/scratchpkg.git
mv scratchpkg $LFS/sources
rm $LFS/sources/scratchpkg/scratchpkg.repo
echo "#lfs" >> $LFS/sources/scratchpkg/scratchpkg.repo
echo "/sources" >> $LFS/sources/scratchpkg/scratchpkg.repo
#echo "SOURCE_DIR=/sources" >> $LFS/sources/scratchpkg/scratchpkg.conf
tar cJf $LFS/sources/scratchpkg.tar.xz $LFS/sources/scratchpkg

rm -rfv $LFS/sources/scratchpkg

chown root:root $LFS/sources/*

set +x
echo '__SCRIPT_DONE__:chapter3/get_packages.sh'
echo '__SCRIPT_BEGIN__:chapter4/directories.sh'
set -euo pipefail
set -x
set -euo pipefail

echo "step:Building limited directory layout"

mkdir -pv $LFS/{lib64,etc,var} $LFS/usr/{bin,lib{,32},sbin}

for i in bin lib lib32 sbin; do
  ln -sv usr/$i $LFS/$i
done

mkdir -pv $LFS/tools

set +x
echo '__SCRIPT_DONE__:chapter4/directories.sh'
echo '__SCRIPT_BEGIN__:chapter4/addlfsuser.sh'
set -euo pipefail
set -x
set -euo pipefail

echo "step:adding LFS user"

groupadd lfs
useradd -s /bin/bash -g lfs -m -k /dev/null lfs
chown -v lfs $LFS/{usr{,/*},var,etc,tools,lib64}
if [ -d "$LFS/sources" ]; then
  chown -Rv lfs:lfs "$LFS/sources"
fi
install -v -m 644 "$PROJECT_ROOT/files/bash_profile" /home/lfs/.bash_profile
install -v -m 644 "$PROJECT_ROOT/files/bashrc" /home/lfs/.bashrc
chown -v lfs:lfs /home/lfs/.bash_profile /home/lfs/.bashrc
su - lfs

set +x
echo '__SCRIPT_DONE__:chapter4/addlfsuser.sh'
echo '__SCRIPT_BEGIN__:chapter4/environment.sh'
set -euo pipefail
set -x
cat > ~/.bash_profile << "EOF"
exec env -i HOME=$HOME TERM=$TERM PS1='\u:\w\$ ' /bin/bash
EOF
cat > ~/.bashrc << "EOF"
set +h
shopt -s expand_aliases
umask 022
LFS=/mnt/lfs
LC_ALL=POSIX
LFS_TGT=x86_64-lfs-linux-gnu
LFS_TGT32=i686-lfs-linux-gnu
PATH=/usr/bin
if [ ! -L /bin ]; then PATH=/bin:$PATH; fi
PATH=$LFS/tools/bin:$PATH
CONFIG_SITE=$LFS/usr/share/config.site
export LFS LC_ALL LFS_TGT LFS_TGT32 PATH
EOF
cat >> ~/.bashrc << "EOF"
export MAKEFLAGS=-j$(nproc)
EOF
source ~/.bash_profile

set +x
echo '__SCRIPT_DONE__:chapter4/environment.sh'
echo '__SCRIPT_BEGIN__:universal/cd_to_sources.sh'
set -euo pipefail
set -x
echo "step:Changing into source directory"
cd $LFS/sources

set +x
echo '__SCRIPT_DONE__:universal/cd_to_sources.sh'
echo '__SCRIPT_BEGIN__:chapter5/binutils-pass1.sh'
set -euo pipefail
set -x
name=binutils
echo "step:Compiling toolchain component binutils-pass 1"

sh autountar $name

cd $name*/
mkdir -v build
cd build

../configure --prefix=$LFS/tools \
             --with-sysroot=$LFS \
             --target=$LFS_TGT   \
             --disable-nls       \
             --enable-gprofng=no \
             --disable-werror    \
             --enable-new-dtags  \
             --enable-default-hash-style=gnu

make
make install

set +x
echo '__SCRIPT_DONE__:chapter5/binutils-pass1.sh'
echo '__SCRIPT_BEGIN__:universal/cd_to_sources.sh'
set -euo pipefail
set -x
echo "step:Changing into source directory"
cd $LFS/sources

set +x
echo '__SCRIPT_DONE__:universal/cd_to_sources.sh'
echo '__SCRIPT_BEGIN__:universal/cleanup.sh'
set -euo pipefail
set -x
echo "step:Cleaning up folders"
cd "$LFS/sources"
rm -rf -- */

set +x
echo '__SCRIPT_DONE__:universal/cleanup.sh'
echo '__SCRIPT_BEGIN__:chapter5/gcc-pass1.sh'
set -euo pipefail
set -x
name=gcc
echo "step:Compiling toolchain component GCC pass 1"

sh autountar $name
cd $name*/
tar -xf ../mpfr-4.2.2.tar.xz
mv -v mpfr-4.2.2 mpfr
tar -xf ../gmp-6.3.0.tar.xz
mv -v gmp-6.3.0 gmp
tar -xf ../mpc-1.3.1.tar.gz
mv -v mpc-1.3.1 mpc

sed -e '/m64=/s/lib64/lib/' \
    -e '/m32=/s/m32=.*/m32=..\/lib32$(call if_multiarch,:i386-linux-gnu)/' \
    -i.orig gcc/config/i386/t-linux64

sed '/STACK_REALIGN_DEFAULT/s/0/(!TARGET_64BIT \&\& TARGET_SSE)/' \
      -i gcc/config/i386/i386.h

mkdir -v build
cd       build

../configure                     \
    --target=$LFS_TGT            \
    --prefix=$LFS/tools          \
    --with-glibc-version=2.43    \
    --with-sysroot=$LFS          \
    --with-newlib                \
    --without-headers            \
    --enable-default-pie         \
    --enable-default-ssp         \
    --enable-initfini-array      \
    --disable-nls                \
    --disable-shared             \
    --enable-multilib            \
    --with-multilib-list=m64,m32 \
    --disable-decimal-float      \
    --disable-threads            \
    --disable-libatomic          \
    --disable-libgomp            \
    --disable-libquadmath        \
    --disable-libssp             \
    --disable-libvtv             \
    --disable-libstdcxx          \
    --enable-languages=c,c++

make 
make install

cat ../gcc/{limitx,glimits,limity}.h  > \
  $($LFS_TGT-gcc -print-file-name=include)/limits.h

set +x
echo '__SCRIPT_DONE__:chapter5/gcc-pass1.sh'
echo '__SCRIPT_BEGIN__:universal/cd_to_sources.sh'
set -euo pipefail
set -x
echo "step:Changing into source directory"
cd $LFS/sources

set +x
echo '__SCRIPT_DONE__:universal/cd_to_sources.sh'
echo '__SCRIPT_BEGIN__:universal/cleanup.sh'
set -euo pipefail
set -x
echo "step:Cleaning up folders"
cd "$LFS/sources"
rm -rf -- */

set +x
echo '__SCRIPT_DONE__:universal/cleanup.sh'
echo '__SCRIPT_BEGIN__:chapter5/linux-headers.sh'
set -euo pipefail
set -x
name=linux
echo "step:Compiling toolchain component linux API headers"

sh autountar $name
cd $name*/

make mrproper
make headers
find usr/include -type f ! -name '*.h' -delete
cp -rv usr/include $LFS/usr

set +x
echo '__SCRIPT_DONE__:chapter5/linux-headers.sh'
echo '__SCRIPT_BEGIN__:universal/cd_to_sources.sh'
set -euo pipefail
set -x
echo "step:Changing into source directory"
cd $LFS/sources

set +x
echo '__SCRIPT_DONE__:universal/cd_to_sources.sh'
echo '__SCRIPT_BEGIN__:universal/cleanup.sh'
set -euo pipefail
set -x
echo "step:Cleaning up folders"
cd "$LFS/sources"
rm -rf -- */

set +x
echo '__SCRIPT_DONE__:universal/cleanup.sh'
echo '__SCRIPT_BEGIN__:chapter5/glibc.sh'
set -euo pipefail
set -x
name=glibc
echo "step:Compiling toolchain component $name"

sh autountar $name
cd $name*/

ln -sfv ../lib/ld-linux-x86-64.so.2 $LFS/lib64
ln -sfv ../lib/ld-linux-x86-64.so.2 $LFS/lib64/ld-lsb-x86-64.so.3

patch -Np1 -i ../glibc-fhs-1.patch
patch -Np1 -i ../glibc-$glibc_version-upstream_fixes-1.patch

mkdir -v build
cd build

echo "rootsbindir=/usr/sbin" > configparms

../configure                             \
      --prefix=/usr                      \
      --host=$LFS_TGT                    \
      --build=$(../scripts/config.guess) \
      --disable-nscd                     \
      libc_cv_slibdir=/usr/lib           \
      --enable-kernel=5.10

make
make DESTDIR=$LFS install

sed '/RTLDLIST=/s@/usr@@g' -i $LFS/usr/bin/ldd

make clean
find .. -name "*.a" -delete

CC="$LFS_TGT-gcc -m32"                   \
CXX="$LFS_TGT-g++ -m32"                  \
../configure                             \
      --prefix=/usr                      \
      --host=$LFS_TGT32                  \
      --build=$(../scripts/config.guess) \
      --disable-nscd                     \
      --with-headers=$LFS/usr/include    \
      --libdir=/usr/lib32                \
      --libexecdir=/usr/lib32            \
      libc_cv_slibdir=/usr/lib32         \
      --enable-kernel=5.10

make

make DESTDIR=$PWD/DESTDIR install
cp -a DESTDIR/usr/lib32 $LFS/usr/
install -vm644 DESTDIR/usr/include/gnu/{lib-names,stubs}-32.h \
               $LFS/usr/include/gnu/
ln -svf ../lib32/ld-linux.so.2 $LFS/lib/ld-linux.so.2

set +x
echo '__SCRIPT_DONE__:chapter5/glibc.sh'
echo '__SCRIPT_BEGIN__:universal/cd_to_sources.sh'
set -euo pipefail
set -x
echo "step:Changing into source directory"
cd $LFS/sources

set +x
echo '__SCRIPT_DONE__:universal/cd_to_sources.sh'
echo '__SCRIPT_BEGIN__:universal/cleanup.sh'
set -euo pipefail
set -x
echo "step:Cleaning up folders"
cd "$LFS/sources"
rm -rf -- */

set +x
echo '__SCRIPT_DONE__:universal/cleanup.sh'
echo '__SCRIPT_BEGIN__:chapter5/libstdcxx.sh'
set -euo pipefail
set -x
name=gcc
echo "step:Compiling toolchain component Libstdc++"

sh autountar $name
cd $name*/

mkdir -v build
cd build

../libstdc++-v3/configure           \
    --host=$LFS_TGT                 \
    --build=$(../config.guess)      \
    --prefix=/usr                   \
    --enable-multilib               \
    --disable-nls                   \
    --disable-libstdcxx-pch         \
    --with-gxx-include-dir=/tools/$LFS_TGT/include/c++/$gcc_version

make
make DESTDIR=$LFS install

rm -v $LFS/usr/lib/lib{stdc++{,exp,fs},supc++}.la

set +x
echo '__SCRIPT_DONE__:chapter5/libstdcxx.sh'
echo '__SCRIPT_BEGIN__:universal/cd_to_sources.sh'
set -euo pipefail
set -x
echo "step:Changing into source directory"
cd $LFS/sources

set +x
echo '__SCRIPT_DONE__:universal/cd_to_sources.sh'
echo '__SCRIPT_BEGIN__:universal/cleanup.sh'
set -euo pipefail
set -x
echo "step:Cleaning up folders"
cd "$LFS/sources"
rm -rf -- */

set +x
echo '__SCRIPT_DONE__:universal/cleanup.sh'
echo '__SCRIPT_BEGIN__:chapter6/m4.sh'
set -euo pipefail
set -x
name=m4
echo "step:Compiling toolchain compenent $name"

sh autountar $name
cd $name*/

./configure --prefix=/usr   \
            --host=$LFS_TGT \
            --build=$(build-aux/config.guess)

make
make DESTDIR=$LFS install

set +x
echo '__SCRIPT_DONE__:chapter6/m4.sh'
echo '__SCRIPT_BEGIN__:universal/cd_to_sources.sh'
set -euo pipefail
set -x
echo "step:Changing into source directory"
cd $LFS/sources

set +x
echo '__SCRIPT_DONE__:universal/cd_to_sources.sh'
echo '__SCRIPT_BEGIN__:universal/cleanup.sh'
set -euo pipefail
set -x
echo "step:Cleaning up folders"
cd "$LFS/sources"
rm -rf -- */

set +x
echo '__SCRIPT_DONE__:universal/cleanup.sh'
echo '__SCRIPT_BEGIN__:chapter6/ncurses.sh'
set -euo pipefail
set -x
echo "step:Compiling toolchain component ncurses"
name=ncurses
sh autountar $name
cd $name*/

mkdir build
pushd build
  ../configure --prefix=$LFS/tools AWK=gawk
  make -C include
  make -C progs tic
  install progs/tic $LFS/tools/bin
popd

./configure --prefix=/usr                \
            --host=$LFS_TGT              \
            --build=$(./config.guess)    \
            --mandir=/usr/share/man      \
            --with-manpage-format=normal \
            --with-shared                \
            --without-normal             \
            --with-cxx-shared            \
            --without-debug              \
            --without-ada                \
            --disable-stripping          \
            AWK=gawk

make
make DESTDIR=$LFS install
ln -sv libncursesw.so $LFS/usr/lib/libncurses.so
sed -e 's/^#if.*XOPEN.*$/#if 1/' \
    -i $LFS/usr/include/curses.h

make distclean
CC="$LFS_TGT-gcc -m32"                \
CXX="$LFS_TGT-g++ -m32"               \
./configure --prefix=/usr             \
            --host=$LFS_TGT32         \
            --build=$(./config.guess) \
            --libdir=/usr/lib32       \
            --mandir=/usr/share/man   \
            --with-shared             \
            --without-normal          \
            --with-cxx-shared         \
            --without-debug           \
            --without-ada             \
            --disable-stripping
make
make DESTDIR=$PWD/DESTDIR TIC_PATH=$(pwd)/build/progs/tic install
ln -sv libncursesw.so DESTDIR/usr/lib32/libncurses.so
cp -Rv DESTDIR/usr/lib32/* $LFS/usr/lib32
rm -rf DESTDIR

set +x
echo '__SCRIPT_DONE__:chapter6/ncurses.sh'
echo '__SCRIPT_BEGIN__:universal/cd_to_sources.sh'
set -euo pipefail
set -x
echo "step:Changing into source directory"
cd $LFS/sources

set +x
echo '__SCRIPT_DONE__:universal/cd_to_sources.sh'
echo '__SCRIPT_BEGIN__:universal/cleanup.sh'
set -euo pipefail
set -x
echo "step:Cleaning up folders"
cd "$LFS/sources"
rm -rf -- */

set +x
echo '__SCRIPT_DONE__:universal/cleanup.sh'
echo '__SCRIPT_BEGIN__:chapter6/bash.sh'
set -euo pipefail
set -x
name=bash
echo "step:Compiling toolchain component $name"

sh autountar "$name"
cd $name*/

./configure --prefix=/usr                      \
            --build=$(sh support/config.guess) \
            --host=$LFS_TGT                    \
            --without-bash-malloc              \
            --docdir=/usr/share/doc/bash-$bash_version

make
make DESTDIR=$LFS install
ln -sv bash $LFS/bin/sh

set +x
echo '__SCRIPT_DONE__:chapter6/bash.sh'
echo '__SCRIPT_BEGIN__:universal/cd_to_sources.sh'
set -euo pipefail
set -x
echo "step:Changing into source directory"
cd $LFS/sources

set +x
echo '__SCRIPT_DONE__:universal/cd_to_sources.sh'
echo '__SCRIPT_BEGIN__:universal/cleanup.sh'
set -euo pipefail
set -x
echo "step:Cleaning up folders"
cd "$LFS/sources"
rm -rf -- */

set +x
echo '__SCRIPT_DONE__:universal/cleanup.sh'
echo '__SCRIPT_BEGIN__:chapter6/coreutils.sh'
set -euo pipefail
set -x
name=coreutils
echo "step:Compiling toolchain component: $name"

sh autountar "$name"
cd $name*/

./configure --prefix=/usr                     \
            --host=$LFS_TGT                   \
            --build=$(build-aux/config.guess) \
            --enable-install-program=hostname \
            --enable-no-install-program=kill,uptime

make
make DESTDIR=$LFS install
mv -v $LFS/usr/bin/chroot              $LFS/usr/sbin
mkdir -pv $LFS/usr/share/man/man8
mv -v $LFS/usr/share/man/man1/chroot.1 $LFS/usr/share/man/man8/chroot.8
sed -i 's/"1"/"8"/'                    $LFS/usr/share/man/man8/chroot.8

set +x
echo '__SCRIPT_DONE__:chapter6/coreutils.sh'
echo '__SCRIPT_BEGIN__:universal/cd_to_sources.sh'
set -euo pipefail
set -x
echo "step:Changing into source directory"
cd $LFS/sources

set +x
echo '__SCRIPT_DONE__:universal/cd_to_sources.sh'
echo '__SCRIPT_BEGIN__:universal/cleanup.sh'
set -euo pipefail
set -x
echo "step:Cleaning up folders"
cd "$LFS/sources"
rm -rf -- */

set +x
echo '__SCRIPT_DONE__:universal/cleanup.sh'
echo '__SCRIPT_BEGIN__:chapter6/diffutils.sh'
set -euo pipefail
set -x
name=diffutils
echo "step:Compiling toolchain component $name"

sh autountar "$name"
cd $name*/

./configure --prefix=/usr   \
            --host=$LFS_TGT \
            gl_cv_func_strcasecmp_works=yes \
            --build=$(./build-aux/config.guess)

make
make DESTDIR=$LFS install

set +x
echo '__SCRIPT_DONE__:chapter6/diffutils.sh'
echo '__SCRIPT_BEGIN__:universal/cd_to_sources.sh'
set -euo pipefail
set -x
echo "step:Changing into source directory"
cd $LFS/sources

set +x
echo '__SCRIPT_DONE__:universal/cd_to_sources.sh'
echo '__SCRIPT_BEGIN__:universal/cleanup.sh'
set -euo pipefail
set -x
echo "step:Cleaning up folders"
cd "$LFS/sources"
rm -rf -- */

set +x
echo '__SCRIPT_DONE__:universal/cleanup.sh'
echo '__SCRIPT_BEGIN__:chapter6/file.sh'
set -euo pipefail
set -x
name=file
echo "step:Compiling toolchain component $name"

sh autountar "$name"
cd $name*/

mkdir build
pushd build
  ../configure --disable-bzlib      \
               --disable-libseccomp \
               --disable-xzlib      \
               --disable-zlib
  make
popd

./configure --prefix=/usr --host=$LFS_TGT --build=$(./config.guess)
make FILE_COMPILE=$(pwd)/build/src/file
make DESTDIR=$LFS install
rm -v $LFS/usr/lib/libmagic.la

set +x
echo '__SCRIPT_DONE__:chapter6/file.sh'
echo '__SCRIPT_BEGIN__:universal/cd_to_sources.sh'
set -euo pipefail
set -x
echo "step:Changing into source directory"
cd $LFS/sources

set +x
echo '__SCRIPT_DONE__:universal/cd_to_sources.sh'
echo '__SCRIPT_BEGIN__:universal/cleanup.sh'
set -euo pipefail
set -x
echo "step:Cleaning up folders"
cd "$LFS/sources"
rm -rf -- */

set +x
echo '__SCRIPT_DONE__:universal/cleanup.sh'
echo '__SCRIPT_BEGIN__:chapter6/findutils.sh'
set -euo pipefail
set -x
name=findutils
echo "step:Compiling toolchain component $name"

sh autountar "$name"
cd $name*/

./configure --prefix=/usr                   \
            --localstatedir=/var/lib/locate \
            --host=$LFS_TGT                 \
            --build=$(build-aux/config.guess)

make
make DESTDIR=$LFS install

set +x
echo '__SCRIPT_DONE__:chapter6/findutils.sh'
echo '__SCRIPT_BEGIN__:universal/cd_to_sources.sh'
set -euo pipefail
set -x
echo "step:Changing into source directory"
cd $LFS/sources

set +x
echo '__SCRIPT_DONE__:universal/cd_to_sources.sh'
echo '__SCRIPT_BEGIN__:universal/cleanup.sh'
set -euo pipefail
set -x
echo "step:Cleaning up folders"
cd "$LFS/sources"
rm -rf -- */

set +x
echo '__SCRIPT_DONE__:universal/cleanup.sh'
echo '__SCRIPT_BEGIN__:chapter6/gawk.sh'
set -euo pipefail
set -x
name=gawk
echo "step:Compiling toolchain component $name"

sh autountar "$name"
cd $name*/

sed -i 's/extras//' Makefile.in
./configure --prefix=/usr   \
            --host=$LFS_TGT \
            --build=$(build-aux/config.guess)

make
make DESTDIR=$LFS install

set +x
echo '__SCRIPT_DONE__:chapter6/gawk.sh'
echo '__SCRIPT_BEGIN__:universal/cd_to_sources.sh'
set -euo pipefail
set -x
echo "step:Changing into source directory"
cd $LFS/sources

set +x
echo '__SCRIPT_DONE__:universal/cd_to_sources.sh'
echo '__SCRIPT_BEGIN__:universal/cleanup.sh'
set -euo pipefail
set -x
echo "step:Cleaning up folders"
cd "$LFS/sources"
rm -rf -- */

set +x
echo '__SCRIPT_DONE__:universal/cleanup.sh'
echo '__SCRIPT_BEGIN__:chapter6/grep.sh'
set -euo pipefail
set -x
name=grep
echo "step:Compiling toolchain component $name"

sh autountar "$name"
cd $name*/

./configure --prefix=/usr   \
            --host=$LFS_TGT \
            --build=$(./build-aux/config.guess)

make
make DESTDIR=$LFS install

set +x
echo '__SCRIPT_DONE__:chapter6/grep.sh'
echo '__SCRIPT_BEGIN__:universal/cd_to_sources.sh'
set -euo pipefail
set -x
echo "step:Changing into source directory"
cd $LFS/sources

set +x
echo '__SCRIPT_DONE__:universal/cd_to_sources.sh'
echo '__SCRIPT_BEGIN__:universal/cleanup.sh'
set -euo pipefail
set -x
echo "step:Cleaning up folders"
cd "$LFS/sources"
rm -rf -- */

set +x
echo '__SCRIPT_DONE__:universal/cleanup.sh'
echo '__SCRIPT_BEGIN__:chapter6/gzip.sh'
set -euo pipefail
set -x
name=gzip
echo "step:Compiling toolchain component $name"

sh autountar "$name"
cd $name*/

./configure --prefix=/usr --host=$LFS_TGT
make
make DESTDIR=$LFS install

set +x
echo '__SCRIPT_DONE__:chapter6/gzip.sh'
echo '__SCRIPT_BEGIN__:universal/cd_to_sources.sh'
set -euo pipefail
set -x
echo "step:Changing into source directory"
cd $LFS/sources

set +x
echo '__SCRIPT_DONE__:universal/cd_to_sources.sh'
echo '__SCRIPT_BEGIN__:universal/cleanup.sh'
set -euo pipefail
set -x
echo "step:Cleaning up folders"
cd "$LFS/sources"
rm -rf -- */

set +x
echo '__SCRIPT_DONE__:universal/cleanup.sh'
echo '__SCRIPT_BEGIN__:chapter6/make.sh'
set -euo pipefail
set -x
name=make
echo "step:Compiling toolchain component $name"

sh autountar "$name"
cd $name*/

./configure --prefix=/usr   \
            --host=$LFS_TGT \
            --build=$(build-aux/config.guess)

make
make DESTDIR=$LFS install

set +x
echo '__SCRIPT_DONE__:chapter6/make.sh'
echo '__SCRIPT_BEGIN__:universal/cd_to_sources.sh'
set -euo pipefail
set -x
echo "step:Changing into source directory"
cd $LFS/sources

set +x
echo '__SCRIPT_DONE__:universal/cd_to_sources.sh'
echo '__SCRIPT_BEGIN__:universal/cleanup.sh'
set -euo pipefail
set -x
echo "step:Cleaning up folders"
cd "$LFS/sources"
rm -rf -- */

set +x
echo '__SCRIPT_DONE__:universal/cleanup.sh'
echo '__SCRIPT_BEGIN__:chapter6/patch.sh'
set -euo pipefail
set -x
name=patch
echo "step:Compiling toolchain component $name"

sh autountar "$name"
cd $name*/

./configure --prefix=/usr   \
            --host=$LFS_TGT \
            --build=$(build-aux/config.guess)

make
make DESTDIR=$LFS install

set +x
echo '__SCRIPT_DONE__:chapter6/patch.sh'
echo '__SCRIPT_BEGIN__:universal/cd_to_sources.sh'
set -euo pipefail
set -x
echo "step:Changing into source directory"
cd $LFS/sources

set +x
echo '__SCRIPT_DONE__:universal/cd_to_sources.sh'
echo '__SCRIPT_BEGIN__:universal/cleanup.sh'
set -euo pipefail
set -x
echo "step:Cleaning up folders"
cd "$LFS/sources"
rm -rf -- */

set +x
echo '__SCRIPT_DONE__:universal/cleanup.sh'
echo '__SCRIPT_BEGIN__:chapter6/sed.sh'
set -euo pipefail
set -x
name=sed
echo "step:Compiling toolchain component $name"

sh autountar "$name"
cd $name*/

./configure --prefix=/usr   \
            --host=$LFS_TGT \
            --build=$(./build-aux/config.guess)

make

make DESTDIR=$LFS install

set +x
echo '__SCRIPT_DONE__:chapter6/sed.sh'
echo '__SCRIPT_BEGIN__:universal/cd_to_sources.sh'
set -euo pipefail
set -x
echo "step:Changing into source directory"
cd $LFS/sources

set +x
echo '__SCRIPT_DONE__:universal/cd_to_sources.sh'
echo '__SCRIPT_BEGIN__:universal/cleanup.sh'
set -euo pipefail
set -x
echo "step:Cleaning up folders"
cd "$LFS/sources"
rm -rf -- */

set +x
echo '__SCRIPT_DONE__:universal/cleanup.sh'
echo '__SCRIPT_BEGIN__:chapter6/tar.sh'
set -euo pipefail
set -x
name=tar
echo "step:Compiling toolchain component $name"

sh autountar "$name"
cd $name*/

./configure --prefix=/usr   \
            --host=$LFS_TGT \
            --build=$(build-aux/config.guess)

make
make DESTDIR=$LFS install

set +x
echo '__SCRIPT_DONE__:chapter6/tar.sh'
echo '__SCRIPT_BEGIN__:universal/cd_to_sources.sh'
set -euo pipefail
set -x
echo "step:Changing into source directory"
cd $LFS/sources

set +x
echo '__SCRIPT_DONE__:universal/cd_to_sources.sh'
echo '__SCRIPT_BEGIN__:universal/cleanup.sh'
set -euo pipefail
set -x
echo "step:Cleaning up folders"
cd "$LFS/sources"
rm -rf -- */

set +x
echo '__SCRIPT_DONE__:universal/cleanup.sh'
echo '__SCRIPT_BEGIN__:chapter6/xz.sh'
set -euo pipefail
set -x
name=xz
echo "step:Compiling toolchain component $name"

sh autountar "$name"
cd $name*/

./configure --prefix=/usr                     \
            --host=$LFS_TGT                   \
            --build=$(build-aux/config.guess) \
            --disable-static                  \
            --docdir=/usr/share/doc/xz-$xz_version

make
make DESTDIR=$LFS install
rm -v $LFS/usr/lib/liblzma.la

set +x
echo '__SCRIPT_DONE__:chapter6/xz.sh'
echo '__SCRIPT_BEGIN__:universal/cd_to_sources.sh'
set -euo pipefail
set -x
echo "step:Changing into source directory"
cd $LFS/sources

set +x
echo '__SCRIPT_DONE__:universal/cd_to_sources.sh'
echo '__SCRIPT_BEGIN__:universal/cleanup.sh'
set -euo pipefail
set -x
echo "step:Cleaning up folders"
cd "$LFS/sources"
rm -rf -- */

set +x
echo '__SCRIPT_DONE__:universal/cleanup.sh'
echo '__SCRIPT_BEGIN__:chapter6/binutils-pass2.sh'
set -euo pipefail
set -x
name=binutils
echo "step:Compiling toolchain component $name"

sh autountar "$name"
cd $name*/

sed '6031s/$add_dir//' -i ltmain.sh

mkdir -v build
cd       build

../configure                   \
    --prefix=/usr              \
    --build=$(../config.guess) \
    --host=$LFS_TGT            \
    --disable-nls              \
    --enable-shared            \
    --enable-gprofng=no        \
    --disable-werror           \
    --enable-64-bit-bfd        \
    --enable-new-dtags         \
    --enable-default-hash-style=gnu

make
make DESTDIR=$LFS install
rm -v $LFS/usr/lib/lib{bfd,ctf,ctf-nobfd,opcodes,sframe}.{a,la}

set +x
echo '__SCRIPT_DONE__:chapter6/binutils-pass2.sh'
echo '__SCRIPT_BEGIN__:universal/cd_to_sources.sh'
set -euo pipefail
set -x
echo "step:Changing into source directory"
cd $LFS/sources

set +x
echo '__SCRIPT_DONE__:universal/cd_to_sources.sh'
echo '__SCRIPT_BEGIN__:universal/cleanup.sh'
set -euo pipefail
set -x
echo "step:Cleaning up folders"
cd "$LFS/sources"
rm -rf -- */

set +x
echo '__SCRIPT_DONE__:universal/cleanup.sh'
echo '__SCRIPT_BEGIN__:chapter6/gcc-pass2.sh'
set -euo pipefail
set -x
name=gcc
echo "step:Compiling toolchain component $name"

sh autountar "$name"
cd $name*/

tar -xf ../mpfr-$mpfr_version.tar.xz
mv -v mpfr-$mpfr_version mpfr
tar -xf ../gmp-$gmp_version.tar.xz
mv -v gmp-$gmp_version gmp
tar -xf ../mpc-$mpc_version.tar.xz
mv -v mpc-$mpc_version mpc

sed -e '/m64=/s/lib64/lib/' \
    -e '/m32=/s/m32=.*/m32=..\/lib32$(call if_multiarch,:i386-linux-gnu)/' \
    -i.orig gcc/config/i386/t-linux64

sed '/STACK_REALIGN_DEFAULT/s/0/(!TARGET_64BIT \&\& TARGET_SSE)/' \
      -i gcc/config/i386/i386.h

mkdir -v build
cd       build

../configure                     \
    --build=$(../config.guess)   \
    --host=$LFS_TGT              \
    --target=$LFS_TGT            \
    --prefix=/usr                \
    --with-build-sysroot=$LFS    \
    --enable-default-pie         \
    --enable-default-ssp         \
    --disable-nls                \
    --enable-multilib            \
    --with-multilib-list=m64,m32 \
    --disable-libatomic          \
    --disable-libgomp            \
    --disable-libquadmath        \
    --disable-libsanitizer       \
    --disable-libssp             \
    --disable-libvtv             \
    --enable-languages=c,c++     \
    LDFLAGS_FOR_TARGET=-L$PWD/$LFS_TGT/libgcc \
    target_configargs=gcc_cv_target_thread_file=posix

make
make DESTDIR=$LFS install
ln -sv gcc $LFS/usr/bin/cc

set +x
echo '__SCRIPT_DONE__:chapter6/gcc-pass2.sh'
echo '__SCRIPT_BEGIN__:universal/cd_to_sources.sh'
set -euo pipefail
set -x
echo "step:Changing into source directory"
cd $LFS/sources

set +x
echo '__SCRIPT_DONE__:universal/cd_to_sources.sh'
echo '__SCRIPT_BEGIN__:universal/cleanup.sh'
set -euo pipefail
set -x
echo "step:Cleaning up folders"
cd "$LFS/sources"
rm -rf -- */

set +x
echo '__SCRIPT_DONE__:universal/cleanup.sh'
echo '__SCRIPT_BEGIN__:chapter7/entering_chroot.sh'
set -euo pipefail
set -x
chroot "$LFS" /usr/bin/env -i   \
    HOME=/root                  \
    TERM="$TERM"                \
    PS1='(lfs chroot) \u:\w\$ ' \
    PATH=/usr/bin:/usr/sbin     \
    MAKEFLAGS="-j$(nproc)"      \
    TESTSUITEFLAGS="-j$(nproc)" \
    /bin/bash --login

set +x
echo '__SCRIPT_DONE__:chapter7/entering_chroot.sh'
echo '__SCRIPT_BEGIN__:chapter7/setting_up_directories.sh'
set -euo pipefail
set -x
echo "step:Setting up Essential Files and Symlinks"

mkdir -pv /{boot,home,mnt,opt,srv}
mkdir -pv /etc/{opt,sysconfig}
mkdir -pv /lib/firmware
mkdir -pv /media/{floppy,cdrom}
mkdir -pv /usr/{,local/}{include,src}
mkdir -pv /usr/lib/locale
mkdir -pv /usr/local/{bin,lib,sbin}
mkdir -pv /usr/{,local/}share/{color,dict,doc,info,locale,man}
mkdir -pv /usr/{,local/}share/{misc,terminfo,zoneinfo}
mkdir -pv /usr/{,local/}share/man/man{1..8}
mkdir -pv /var/{cache,local,log,mail,opt,spool}
mkdir -pv /var/lib/{color,misc,locate}
mkdir -pv /var/cache/scratchpkg
mkdir -pv /var/lib/scratchpkg/db
ln -sfv /run /var/run
ln -sfv /run/lock /var/lock

install -dv -m 0750 /root
install -dv -m 1777 /tmp /var/tmp

ln -sv /proc/self/mounts /etc/mtab

cat > /etc/hosts << EOF
127.0.0.1  localhost $(hostname)
::1        localhost
EOF

cat > /etc/passwd << "EOF"
root:x:0:0:root:/root:/bin/bash
bin:x:1:1:bin:/dev/null:/usr/bin/false
daemon:x:6:6:Daemon User:/dev/null:/usr/bin/false
messagebus:x:18:18:D-Bus Message Daemon User:/run/dbus:/usr/bin/false
systemd-journal-gateway:x:73:73:systemd Journal Gateway:/:/usr/bin/false
systemd-journal-remote:x:74:74:systemd Journal Remote:/:/usr/bin/false
systemd-journal-upload:x:75:75:systemd Journal Upload:/:/usr/bin/false
systemd-network:x:76:76:systemd Network Management:/:/usr/bin/false
systemd-resolve:x:77:77:systemd Resolver:/:/usr/bin/false
systemd-timesync:x:78:78:systemd Time Synchronization:/:/usr/bin/false
systemd-coredump:x:79:79:systemd Core Dumper:/:/usr/bin/false
uuidd:x:80:80:UUID Generation Daemon User:/dev/null:/usr/bin/false
systemd-oom:x:81:81:systemd Out Of Memory Daemon:/:/usr/bin/false
nobody:x:65534:65534:Unprivileged User:/dev/null:/usr/bin/false
EOF

cat > /etc/group << "EOF"
root:x:0:
bin:x:1:daemon
sys:x:2:
kmem:x:3:
tape:x:4:
tty:x:5:
daemon:x:6:
floppy:x:7:
disk:x:8:
lp:x:9:
dialout:x:10:
audio:x:11:
video:x:12:
utmp:x:13:
clock:x:14:
cdrom:x:15:
adm:x:16:
messagebus:x:18:
systemd-journal:x:23:
input:x:24:
mail:x:34:
kvm:x:61:
systemd-journal-gateway:x:73:
systemd-journal-remote:x:74:
systemd-journal-upload:x:75:
systemd-network:x:76:
systemd-resolve:x:77:
systemd-timesync:x:78:
systemd-coredump:x:79:
uuidd:x:80:
systemd-oom:x:81:
wheel:x:97:
users:x:999:
nogroup:x:65534:
EOF

exec /usr/bin/bash --login

touch /var/log/{btmp,lastlog,faillog,wtmp}
chgrp -v utmp /var/log/lastlog
chmod -v 664  /var/log/lastlog
chmod -v 600  /var/log/btmp

set +x
echo '__SCRIPT_DONE__:chapter7/setting_up_directories.sh'
echo '__SCRIPT_BEGIN__:chapter7/change_source_directory.sh'
set -euo pipefail
set -x
cd /sources

set +x
echo '__SCRIPT_DONE__:chapter7/change_source_directory.sh'
echo '__SCRIPT_BEGIN__:chapter7/gettext.sh'
set -euo pipefail
set -x
name=gettext
echo "step:Installing $name"

sh autountar "$name"
cd $name*/

./configure --disable-shared
make
cp -v gettext-tools/src/{msgfmt,msgmerge,xgettext} /usr/bin

set +x
echo '__SCRIPT_DONE__:chapter7/gettext.sh'
echo '__SCRIPT_BEGIN__:chapter7/change_source_directory.sh'
set -euo pipefail
set -x
cd /sources

set +x
echo '__SCRIPT_DONE__:chapter7/change_source_directory.sh'
echo '__SCRIPT_BEGIN__:chapter7/cleanup.sh'
set -euo pipefail
set -x
echo "step:Cleaning up folders"
cd "/sources"
rm -rf -- */

set +x
echo '__SCRIPT_DONE__:chapter7/cleanup.sh'
echo '__SCRIPT_BEGIN__:chapter7/bison.sh'
set -euo pipefail
set -x
name=bison
echo "step:Installing $name"

sh autountar "$name"
cd $name*/

./configure --prefix=/usr \
            --docdir=/usr/share/doc/bison-$bison_version
make
make install

set +x
echo '__SCRIPT_DONE__:chapter7/bison.sh'
echo '__SCRIPT_BEGIN__:chapter7/change_source_directory.sh'
set -euo pipefail
set -x
cd /sources

set +x
echo '__SCRIPT_DONE__:chapter7/change_source_directory.sh'
echo '__SCRIPT_BEGIN__:chapter7/cleanup.sh'
set -euo pipefail
set -x
echo "step:Cleaning up folders"
cd "/sources"
rm -rf -- */

set +x
echo '__SCRIPT_DONE__:chapter7/cleanup.sh'
echo '__SCRIPT_BEGIN__:chapter7/perl.sh'
set -euo pipefail
set -x
name=perl
echo "step:Installing $name"

sh autountar "$name"
cd $name*/

sh Configure -des                                         \
             -D prefix=/usr                               \
             -D vendorprefix=/usr                         \
             -D useshrplib                                \
             -D privlib=/usr/lib/perl5/${perl_version%.*}/core_perl     \
             -D archlib=/usr/lib/perl5/${perl_version%.*}/core_perl     \
             -D sitelib=/usr/lib/perl5/${perl_version%.*}/site_perl     \
             -D sitearch=/usr/lib/perl5/${perl_version%.*}/site_perl    \
             -D vendorlib=/usr/lib/perl5/${perl_version%.*}/vendor_perl \
             -D vendorarch=/usr/lib/perl5/${perl_version%.*}/vendor_perl

make
make install

set +x
echo '__SCRIPT_DONE__:chapter7/perl.sh'
echo '__SCRIPT_BEGIN__:chapter7/change_source_directory.sh'
set -euo pipefail
set -x
cd /sources

set +x
echo '__SCRIPT_DONE__:chapter7/change_source_directory.sh'
echo '__SCRIPT_BEGIN__:chapter7/cleanup.sh'
set -euo pipefail
set -x
echo "step:Cleaning up folders"
cd "/sources"
rm -rf -- */

set +x
echo '__SCRIPT_DONE__:chapter7/cleanup.sh'
echo '__SCRIPT_BEGIN__:chapter7/python.sh'
set -euo pipefail
set -x
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

set +x
echo '__SCRIPT_DONE__:chapter7/python.sh'
echo '__SCRIPT_BEGIN__:chapter7/change_source_directory.sh'
set -euo pipefail
set -x
cd /sources

set +x
echo '__SCRIPT_DONE__:chapter7/change_source_directory.sh'
echo '__SCRIPT_BEGIN__:chapter7/cleanup.sh'
set -euo pipefail
set -x
echo "step:Cleaning up folders"
cd "/sources"
rm -rf -- */

set +x
echo '__SCRIPT_DONE__:chapter7/cleanup.sh'
echo '__SCRIPT_BEGIN__:chapter7/texinfo.sh'
set -euo pipefail
set -x
name=texinfo
echo "step:Installing $name"

sh autountar "$name"
cd $name*/

./configure --prefix=/usr
make 
make install

set +x
echo '__SCRIPT_DONE__:chapter7/texinfo.sh'
echo '__SCRIPT_BEGIN__:chapter7/change_source_directory.sh'
set -euo pipefail
set -x
cd /sources

set +x
echo '__SCRIPT_DONE__:chapter7/change_source_directory.sh'
echo '__SCRIPT_BEGIN__:chapter7/cleanup.sh'
set -euo pipefail
set -x
echo "step:Cleaning up folders"
cd "/sources"
rm -rf -- */

set +x
echo '__SCRIPT_DONE__:chapter7/cleanup.sh'
echo '__SCRIPT_BEGIN__:chapter7/util-linux.sh'
set -euo pipefail
set -x
name=util-linux
echo "step:Installing $name"

sh autountar "$name"
cd $name*/

mkdir -pv /var/lib/hwclock

./configure --libdir=/usr/lib     \
            --runstatedir=/run    \
            --disable-chfn-chsh   \
            --disable-login       \
            --disable-nologin     \
            --disable-su          \
            --disable-setpriv     \
            --disable-runuser     \
            --disable-pylibmount  \
            --disable-static      \
            --disable-liblastlog2 \
            --without-python      \
            ADJTIME_PATH=/var/lib/hwclock/adjtime \
            --docdir=/usr/share/doc/util-linux-$util_linux_version

make 
make install

make distclean
CC="gcc -m32"                        \
./configure --host=i686-pc-linux-gnu \
            --libdir=/usr/lib32      \
            --runstatedir=/run       \
            --disable-chfn-chsh      \
            --disable-login          \
            --disable-nologin        \
            --disable-su             \
            --disable-setpriv        \
            --disable-runuser        \
            --disable-pylibmount     \
            --disable-static         \
            --disable-liblastlog2    \
            --without-python         \
            ADJTIME_PATH=/var/lib/hwclock/adjtime

make
make DESTDIR=$PWD/DESTDIR install
cp -Rv DESTDIR/usr/lib32/* /usr/lib32
rm -rf DESTDIR

set +x
echo '__SCRIPT_DONE__:chapter7/util-linux.sh'
echo '__SCRIPT_BEGIN__:chapter7/change_source_directory.sh'
set -euo pipefail
set -x
cd /sources

set +x
echo '__SCRIPT_DONE__:chapter7/change_source_directory.sh'
echo '__SCRIPT_BEGIN__:chapter7/cleanup.sh'
set -euo pipefail
set -x
echo "step:Cleaning up folders"
cd "/sources"
rm -rf -- */

set +x
echo '__SCRIPT_DONE__:chapter7/cleanup.sh'
echo '__SCRIPT_BEGIN__:chapter7/cleaningup.sh'
set -euo pipefail
set -x
rm -rf /usr/share/{info,man,doc}/*
find /usr/{lib{,32},libexec} -name \*.la -delete
rm -rf /tools

set +x
echo '__SCRIPT_DONE__:chapter7/cleaningup.sh'
echo '__SCRIPT_BEGIN__:chapter8/scratchpkg.sh'
set -euo pipefail
set -x
name=scratchpkg
echo "step:Installing $name"
sh autountar scratchpkg
sh autountar "$name"
cd $name*/

./install

set +x
echo '__SCRIPT_DONE__:chapter8/scratchpkg.sh'
echo '__SCRIPT_BEGIN__:chapter7/change_source_directory.sh'
set -euo pipefail
set -x
cd /sources

set +x
echo '__SCRIPT_DONE__:chapter7/change_source_directory.sh'
echo '__SCRIPT_BEGIN__:chapter7/cleanup.sh'
set -euo pipefail
set -x
echo "step:Cleaning up folders"
cd "/sources"
rm -rf -- */

set +x
echo '__SCRIPT_DONE__:chapter7/cleanup.sh'
echo '__SCRIPT_BEGIN__:chapter8/install_system.sh'
set -euo pipefail
set -x
echo "step:Installing system packages"
scratch install man-pages iana-etc glibc zlib bzip2 xz lz4 zstd file readline pcre2 m4 bc flex tcl expect dejagnu pkgconf binutils gmp mpfr mpc isl attr acl libcap libxcrypt shadow gcc ncurses sed psmisc gettext bison grep bash libtool gdbm gperf expat inetutils less perl autoconf automake openssl libelf libffi sqlite python flit-core packaging wheel setuptools ninja meson kmod coreutils diffutils gawk findutils groff grub gzip iproute kbd libpipeline make patch tar texinfo nano markupsafe jinja systemd dbus man-db prcps-ng util-linux e2fsprogs wget

set +x
echo '__SCRIPT_DONE__:chapter8/install_system.sh'
echo '__SCRIPT_BEGIN__:chapter9/critiques.sh'
set -euo pipefail
set -x
echo "step: Critiquing"
cat > /etc/systemd/network/10-eth-dhcp.network << "EOF"
[Match]
Name=<network-device-name>

[Network]
DHCP=ipv4

[DHCPv4]
UseDomains=true
EOF

cat > /etc/resolv.conf << "EOF"
# Begin /etc/resolv.conf

domain <Your Domain Name>
nameserver <IP address of your primary nameserver>
nameserver <IP address of your secondary nameserver>

# End /etc/resolv.conf
EOF

cat > /etc/hosts << "EOF"
# Begin /etc/hosts

<192.168.0.2> <FQDN> [alias1] [alias2] ...
::1       ip6-localhost ip6-loopback
ff02::1   ip6-allnodes
ff02::2   ip6-allrouters

# End /etc/hosts
EOF

cat > /etc/locale.conf << "EOF"
LANG=en_US.UTF
EOF


cat > /etc/profile << "EOF"
# Begin /etc/profile

for i in $(locale); do
  unset ${i%=*}
done

if [[ "$TERM" = linux ]]; then
  export LANG=C.UTF-8
else
  source /etc/locale.conf

  for i in $(locale); do
    key=${i%=*}
    if [[ -v $key ]]; then
      export $key
    fi
  done
fi

# End /etc/profile
EOF

cat > /etc/inputrc << "EOF"
# Begin /etc/inputrc
# Modified by Chris Lynn <roryo@roryo.dynup.net>

# Allow the command prompt to wrap to the next line
set horizontal-scroll-mode Off

# Enable 8-bit input
set meta-flag On
set input-meta On

# Turns off 8th bit stripping
set convert-meta Off

# Keep the 8th bit for display
set output-meta On

# none, visible or audible
set bell-style none

# All of the following map the escape sequence of the value
# contained in the 1st argument to the readline specific functions
"\eOd": backward-word
"\eOc": forward-word

# for linux console
"\e[1~": beginning-of-line
"\e[4~": end-of-line
"\e[5~": beginning-of-history
"\e[6~": end-of-history
"\e[3~": delete-char
"\e[2~": quoted-insert

# for xterm
"\eOH": beginning-of-line
"\eOF": end-of-line

# for Konsole
"\e[H": beginning-of-line
"\e[F": end-of-line

# End /etc/inputrc
EOF


cat > /etc/shells << "EOF"
# Begin /etc/shells

/bin/sh
/bin/bash

# End /etc/shells
EOF


mkdir -pv /etc/systemd/coredump.conf.d
cat > /etc/systemd/coredump.conf.d/maxuse.conf << EOF
[Coredump]
MaxUse=5G
EOF

set +x
echo '__SCRIPT_DONE__:chapter9/critiques.sh'
echo '__SCRIPT_BEGIN__:chapter10/kernel.sh'
set -euo pipefail
set -x
name=linux
echo "step:Installing $kernel"

scratch install linux-desktop

set +x
echo '__SCRIPT_DONE__:chapter10/kernel.sh'
echo '__SCRIPT_BEGIN__:chapter10/boot.sh'
set -euo pipefail
set -x
echo "step:Making bootable"
grub-install --target=x86_64-efi --removable
grub-mkconfig -o /boot/grub/grub.cfg

set +x
echo '__SCRIPT_DONE__:chapter10/boot.sh'
echo '__SCRIPT_BEGIN__:chapter10/done.sh'
set -euo pipefail
set -x
echo "step:Finished...choose additions on next page"

set +x
echo '__SCRIPT_DONE__:chapter10/done.sh'
exit
exit
