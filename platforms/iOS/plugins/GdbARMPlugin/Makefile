PREREQUISITES:= ../gdbarm32/bfd/libbfd.a \
				../gdbarm32/libiberty/libiberty.a \
				../gdbarm32/opcodes/libopcodes.a \
				../gdbarm32/sim/arm/libsim.a \
				../gdbarm32/zlib/libz.a

INCDIRS:=../../processors/ARM/gdb-7.10/include \
         ../../processors/ARM/gdb-7.10/sim/arm \
         ../../processors/ARM/gdb-7.10/bfd \
         ../gdbarm32/bfd
INCFIRST:=../../processors/ARM/gdb-7.10/include \
          ../../processors/ARM/gdb-7.10/sim/arm \
          ../../processors/ARM/gdb-7.10/bfd \
          ../gdbarm32/bfd
EXTRALIBS:=-L../gdbarm32/bfd -L../gdbarm32/libiberty -L../gdbarm32/opcodes \
           -L../gdbarm32/sim/arm -L../gdbarm32/zlib \
           -lbfd -liberty -lopcodes -lopcodes -lsim -lz

include ../common/Makefile.plugin

INCDIRS:= $(INCFIRST) $(MAKERVMDIR) $(CROSSVMDIR) $(MAKERDIR) $(OSXPLGDIR) $(OSXVMDIR) $(CROSSDIR)
INCLUDES:= $(addprefix -I,. $(INCDIRS))