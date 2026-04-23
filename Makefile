# MertOS Makefile v2
# Cross-compiler: x86_64-elf-gcc (WSL2'de kurulu)
# Build: make        → kernel.elf + MertOS.iso
# Run:   make run    → QEMU ile başlat
# Debug: make debug  → GDB server (:1234)
# Clean: make clean

# ============================================================
# Araçlar
# x86_64-elf-gcc yoksa x86_64-linux-gnu-gcc kullan
# ============================================================
ifneq ($(shell which x86_64-elf-g++ 2>/dev/null),)
  CXX := x86_64-elf-g++
  CC  := x86_64-elf-gcc
  LD  := x86_64-elf-ld
else
  CXX := x86_64-linux-gnu-g++
  CC  := x86_64-linux-gnu-gcc
  LD  := x86_64-linux-gnu-ld
endif
NASM    := nasm
GRUB    := grub-mkrescue
QEMU    := qemu-system-x86_64

# ============================================================
# Derleyici Bayrakları
# ============================================================
CXXFLAGS := \
    -std=c++17              \
    -ffreestanding          \
    -fno-exceptions         \
    -fno-rtti               \
    -fno-stack-protector    \
    -mno-red-zone           \
    -mno-mmx                \
    -mno-sse                \
    -mno-sse2               \
    -Wall                   \
    -Wextra                 \
    -O2                     \
    -g                      \
    -nostdinc               \
    -isystem $(shell $(CC) -print-file-name=include) \
    -I.                     \
    -Ikernel                \
    -Ilibc/include

NASMFLAGS := -f elf64 -g -F dwarf

LIBGCC  := $(shell $(CC) -print-libgcc-file-name)

LDFLAGS := \
    -T kernel.ld            \
    -nostdlib               \
    --no-dynamic-linker     \
    -z max-page-size=0x1000

# ============================================================
# Kaynak Dosyaları
# ============================================================
ASM_SOURCES := \
    kernel/arch/x86_64/entry.asm            \
    kernel/arch/x86_64/isr_stubs.asm        \
    kernel/process/context_switch.asm

CXX_SOURCES := \
    kernel/kernel.cpp                       \
    kernel/arch/x86_64/gdt.cpp              \
    kernel/arch/x86_64/idt.cpp              \
    kernel/drivers/driver.cpp               \
    kernel/drivers/vga.cpp                  \
    kernel/drivers/keyboard.cpp             \
    kernel/drivers/mouse.cpp                \
    kernel/drivers/framebuffer.cpp          \
    kernel/memory/pmm.cpp                   \
    kernel/memory/vmm.cpp                   \
    kernel/memory/heap.cpp                  \
    kernel/process/process.cpp              \
    kernel/shell/shell.cpp                  \
    kernel/desktop/window.cpp               \
    kernel/desktop/splash.cpp               \
    kernel/desktop/desktop.cpp              \
    kernel/fs/vfs.cpp                       \
    kernel/desktop/app.cpp                  \
    kernel/desktop/terminal_app.cpp         \
    kernel/desktop/file_manager.cpp         \
    kernel/desktop/settings_app.cpp         \
    kernel/desktop/msc_app.cpp

# Object dosyaları
ASM_OBJECTS := $(ASM_SOURCES:.asm=.asm.o)
CXX_OBJECTS := $(CXX_SOURCES:.cpp=.o)
ALL_OBJECTS := $(ASM_OBJECTS) $(CXX_OBJECTS)

# ============================================================
# Ana Hedefler
# ============================================================
.PHONY: all clean run debug iso size disasm

all: MertOS.iso

# ---- ELF binary ----
kernel.elf: $(ALL_OBJECTS)
	@echo "[LD]  Linking kernel.elf..."
	$(LD) $(LDFLAGS) -o $@ $^ $(LIBGCC)
	@echo "[OK]  kernel.elf built ($(shell du -sh kernel.elf 2>/dev/null | cut -f1))"

# ---- Bootable ISO ----
MertOS.iso: kernel.elf
	@echo "[ISO] Building bootable ISO..."
	@cp kernel.elf iso/boot/kernel.elf
	@$(GRUB) -o MertOS.iso iso/ 2>/dev/null
	@echo "[OK]  MertOS.iso ready"

# ============================================================
# Derleme Kuralları
# ============================================================
%.asm.o: %.asm
	@echo "[ASM] $<"
	@$(NASM) $(NASMFLAGS) $< -o $@

%.o: %.cpp
	@echo "[CXX] $<"
	@$(CXX) $(CXXFLAGS) -c $< -o $@

# ============================================================
# QEMU Çalıştırma
# ============================================================
QEMU_FLAGS := \
    -cdrom MertOS.iso           \
    -m 256M                     \
    -serial stdio               \
    -no-reboot                  \
    -no-shutdown                \
    -display sdl                \
    -vga std                    \
    -global VGA.vgamem_mb=16    \
    -usb                        \
    -device usb-tablet

# Sade çalıştırma
run: MertOS.iso
	@echo "[RUN] Starting MertOS in QEMU..."
	@$(QEMU) $(QEMU_FLAGS)

# Grafik olmadan (headless/WSL için)
run-headless: MertOS.iso
	@echo "[RUN] Starting MertOS in QEMU (headless)..."
	@$(QEMU) -cdrom MertOS.iso -m 256M -serial stdio -no-reboot -no-shutdown -display none

# GDB debug sunucusu
debug: MertOS.iso
	@echo "[DBG] QEMU + GDB server on localhost:1234"
	@echo "      GDB komutları:"
	@echo "        target remote localhost:1234"
	@echo "        symbol-file kernel.elf"
	@echo "        b kmain"
	@echo "        c"
	@$(QEMU) $(QEMU_FLAGS) -s -S

# ============================================================
# Yardımcı Hedefler
# ============================================================
clean:
	@echo "[CLN] Cleaning..."
	@rm -f $(ALL_OBJECTS) kernel.elf MertOS.iso iso/boot/kernel.elf
	@echo "[OK]  Done"

size: kernel.elf
	@$(CC:%gcc=%size) -A kernel.elf

disasm: kernel.elf
	@$(CC:%gcc=%objdump) -d -M intel kernel.elf | less

# Bağımlılık ağacını göster
deps:
	@echo "ASM objects: $(ASM_OBJECTS)"
	@echo "CXX objects: $(CXX_OBJECTS)"
