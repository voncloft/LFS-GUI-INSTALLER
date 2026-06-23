set +h
umask 022

export LFS=/mnt/lfs
export LC_ALL=POSIX
export LFS_TGT=x86_64-lfs-linux-gnu
export LFS_TGT32=i686-lfs-linux-gnu

PATH=/usr/bin
if [ ! -L /bin ]; then
  PATH=/bin:$PATH
fi
PATH=$LFS/tools/bin:$PATH

export PATH
export CONFIG_SITE=$LFS/usr/share/config.site
