set -euo pipefail

export LFS=/mnt/LFS
umask 022
su - lfs
source ../files/bash_profile
source ../files/bashrc
