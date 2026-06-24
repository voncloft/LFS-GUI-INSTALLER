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
if [ -n "${TARGET_SCRIPTS:-}" ]; then
    AUTOUNTAR_PATH="$(cd -- "$TARGET_SCRIPTS/.." && pwd)/tools/autountar.sh"
    printf "alias autountar='%s'\n" "$AUTOUNTAR_PATH" >> ~/.bashrc
fi
source ~/.bash_profile
