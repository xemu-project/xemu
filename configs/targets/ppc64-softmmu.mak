TARGET_ARCH=ppc64
TARGET_BASE_ARCH=ppc
TARGET_BIG_ENDIAN=y
TARGET_SUPPORTS_MTTCG=y
TARGET_KVM_HAVE_GUEST_DEBUG=y
TARGET_XML_FILES= gdb-xml/power64-core.xml gdb-xml/power-fpu.xml gdb-xml/power-altivec.xml gdb-xml/power-spe.xml gdb-xml/power-vsx.xml
# all boards require libfdt
TARGET_NEED_FDT=y
