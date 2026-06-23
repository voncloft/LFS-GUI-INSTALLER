set -euo pipefail

export LFS=/mnt/lfs
umask 022
su - lfs
source ../files/bash_profile
source ../files/bashrc
