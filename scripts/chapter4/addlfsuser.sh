set -euo pipefail

source  ../universal/default.sh
echo "step:adding LFS user"

groupadd lfs
useradd -s /bin/bash -g lfs -m -k /dev/null lfs
chown -v lfs $LFS/{usr{,/*},var,etc,tools,lib64}
