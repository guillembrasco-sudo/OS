xorriso -as mkisofs \
  -R \
  -b boot/grub/i386-pc/eltorito.img \
  -no-emul-boot \
  -boot-load-size 4 \
  -boot-info-table \
  -o kernel.iso \
  iso

/mingw64/bin/qemu-system-x86_64 -cdrom kernel.iso -serial stdio -d int,cpu_reset,guest_errors -D qemu.log -no-reboot -no-shutdown

find /boot/kernel.bin
root (cd)
kernel /boot/kernel.bin
boot


mkdir -p iso/boot/grub/i386-pc
grub-mkimage \
  -O i386-pc \
  -d /c/grub2/i386-pc \
  -p /boot/grub \
  -o iso/boot/grub/i386-pc/core.img \
  biosdisk iso9660 normal configfile multiboot
cat /c/grub2/i386-pc/cdboot.img \
  iso/boot/grub/i386-pc/core.img \
  > iso/boot/grub/i386-pc/eltorito.img

/mingw64/bin/qemu-system-x86_64 \
  -cdrom kernel.iso \
  -serial stdio \
  -d int,cpu_reset,guest_errors \
  -D qemu.log \
  -no-reboot \
  -no-shutdown