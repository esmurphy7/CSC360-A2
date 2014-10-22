$(common-objpfx)csu/elf-init.o: elf-init.c \
 ../include/libc-symbols.h $(common-objpfx)config.h \
 ../sysdeps/generic/symbol-hacks.h \
 /usr/lib/gcc/x86_64-linux-gnu/4.6/include/stddef.h

../include/libc-symbols.h:

$(common-objpfx)config.h:

../sysdeps/generic/symbol-hacks.h:

/usr/lib/gcc/x86_64-linux-gnu/4.6/include/stddef.h:
