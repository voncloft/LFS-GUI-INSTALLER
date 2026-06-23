set -euo pipefail

source  ../universal/default.sh
echo "step:Building limited directory layout"

mkdir -pv $LFS/{lib64,etc,var} $LFS/usr/{bin,lib{,32},sbin}

for i in bin lib lib32 sbin; do
  ln -sv usr/$i $LFS/$i
done

mkdir -pv $LFS/tools
