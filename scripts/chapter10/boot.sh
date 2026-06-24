echo "step:Making bootable"
grub-install --target=x86_64-efi --removable
grub-mkconfig -o /boot/grub/grub.cfg
