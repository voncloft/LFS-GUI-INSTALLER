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
#su - lfs
