set -euo pipefail

echo "step:Getting Packages"
mkdir -v $LFS/sources
chmod -v a+wt $LFS/sources

#wget https://www.linuxfromscratch.org/mlfs/view/dev/wget-list-systemd
#wget --input-file=wget-list-systemd --continue --directory-prefix=$LFS/sources

#pushd $LFS/sources
#  md5sum -c md5sums
#popd
wget https://sourceware.org/pub/binutils/releases/binutils-2.46.0.tar.xz
mv binutils*.tar.xz $LFS/sources
chown root:root $LFS/sources/*
