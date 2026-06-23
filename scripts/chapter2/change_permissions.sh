set -euo pipefail
export LFS=/mnt/lfs
echo "step:Changing permissions of root drive"
chown root:root $LFS
chmod 755 $LFS
