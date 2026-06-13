include Makefile.inc

KERNEL       := kernel.bin

# ---- memory manager selection ----
#   make         → buddy allocator (default)
#   make bitmap  → bitmap allocator
MM_SRC       ?= memory/buddy_mm.c
MM_OBJ        = $(MM_SRC:.c=.o)

# ---- fuentes ----
C_SRCS       := $(wildcard *.c) \
                $(wildcard drivers/*.c) \
                $(wildcard syscall/*.c) \
                $(wildcard benchmark/*.c) \
                $(wildcard utils/*.c) \
                $(wildcard interrupts/*.c) \
                $(wildcard process/*.c) \
                $(wildcard scheduler/*.c) \
                $(wildcard pipes/*.c) \
                $(wildcard sem/*.c)

ASM_SRCS     := $(wildcard asm/*.asm)

C_OBJS       := $(C_SRCS:.c=.o)
ASM_OBJS     := $(ASM_SRCS:.asm=.o)

LOADER       := loader.asm
LOADER_OBJ   := $(LOADER:.asm=.o)

STATICLIBS   :=
INCLUDES     := -I./include

# --- Flags extra para habilitar SSE en kernel x86_64 ---
# No dependemos de Makefile.inc: sumamos por si allí ya hay otras flags.
GCCFLAGS    += -m64 -msse -msse2 -mfpmath=sse -mno-red-zone \
               -ffreestanding -fno-builtin -fno-omit-frame-pointer \
               -fno-stack-protector -fno-pic -fno-pie
# Si usás NASM/YASM, asegurate de compilar a ELF64
ASMFLAGS    += -f elf64
# Enlazado típico de kernel freestanding
LDFLAGS     += -nostdlib -z max-page-size=0x1000

# ---- reglas ----
all: $(KERNEL)

# Compila con el Buddy System
buddy:
	$(MAKE) MM_SRC=memory/buddy_mm.c

# Compila con el Bitmapped Memory Manager
bitmap:
	$(MAKE) MM_SRC=memory/bitmap_mm.c

$(KERNEL): $(LOADER_OBJ) $(C_OBJS) $(MM_OBJ) $(ASM_OBJS) $(STATICLIBS)
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

%.o: %.c
	$(GCC) $(GCCFLAGS) $(INCLUDES) -c $< -o $@

%.o: %.asm
	$(ASM) $(ASMFLAGS) $< -o $@

$(LOADER_OBJ): $(LOADER)
	$(ASM) $(ASMFLAGS) $< -o $@

clean:
	rm -f $(C_OBJS) $(ASM_OBJS) $(LOADER_OBJ) $(KERNEL) \
	      memory/*.o drivers/*.o syscall/*.o benchmark/*.o utils/*.o \
	      interrupts/*.o process/*.o scheduler/*.o pipes/*.o sem/*.o asm/*.o

.PHONY: all buddy clean
