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
mv $LFS/sources/*/* .
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
