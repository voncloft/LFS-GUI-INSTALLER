set -euo pipefall
source  ../universal/default.sh

echo "step:Getting Packages"
mkdir -v $LFS/sources
chmod -v a+wt $LFS/sources

wget https://www.linuxfromscratch.org/mlfs/view/dev/wget-list-systemd
wget --input-file=wget-list-systemd --continue --directory-prefix=$LFS/sources

pushd $LFS/sources
  md5sum -c md5sums
popd

chown root:root $LFS/sources/*
