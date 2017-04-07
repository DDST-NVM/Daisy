qemu-system-x86_64 -hda ../rootfs.img -kernel arch/x86/boot/bzImage -append "root=/dev/hda console=ttyS0 rdinit=sbin/init noapic" -m 6000 -nographic

