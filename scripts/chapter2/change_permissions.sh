set -euo pipefail

source ../universal/default.sh

echo "step:Changing permissions of root drive"
chown root:root $LFS
chmod 755 $LFS
