#!/bin/bash
# setup_wsl.sh — MertOS Geliştirme Ortamı Kurulumu (WSL2/Ubuntu)
# Çalıştır: bash setup_wsl.sh

set -e
echo "=== MertOS WSL2 Geliştirme Ortamı Kurulumu ==="

# Temel araçlar
sudo apt update && sudo apt install -y \
    build-essential     \
    nasm                \
    qemu-system-x86     \
    grub-pc-bin         \
    grub-common         \
    xorriso             \
    mtools              \
    gdb                 \
    git

# Cross-compiler kurulumu
# Seçenek 1: apt'den (kolay ama eski olabilir)
echo "[*] Cross-compiler kurulumu..."
sudo apt install -y gcc-x86-64-linux-gnu 2>/dev/null || true

# Seçenek 2: Kaynak koddan derleme (daha güvenilir)
# Eğer apt'den geleni beğenmezsen bu scripti kullan:
if ! command -v x86_64-elf-gcc &>/dev/null; then
    echo "[*] x86_64-elf-gcc bulunamadı, kaynak koddan kurulacak..."
    echo "    Bu işlem ~30 dakika sürebilir."

    # Bağımlılıklar
    sudo apt install -y \
        libgmp-dev libmpfr-dev libmpc-dev \
        texinfo wget

    BINUTILS_VER="2.41"
    GCC_VER="13.2.0"
    PREFIX="$HOME/cross"
    TARGET="x86_64-elf"

    mkdir -p "$PREFIX" /tmp/cross-build
    cd /tmp/cross-build

    # Binutils
    wget -q "https://ftp.gnu.org/gnu/binutils/binutils-${BINUTILS_VER}.tar.gz"
    tar xf "binutils-${BINUTILS_VER}.tar.gz"
    mkdir build-binutils && cd build-binutils
    "../binutils-${BINUTILS_VER}/configure" \
        --target="$TARGET" --prefix="$PREFIX" \
        --with-sysroot --disable-nls --disable-werror
    make -j$(nproc) && make install
    cd ..

    # GCC
    wget -q "https://ftp.gnu.org/gnu/gcc/gcc-${GCC_VER}/gcc-${GCC_VER}.tar.gz"
    tar xf "gcc-${GCC_VER}.tar.gz"
    mkdir build-gcc && cd build-gcc
    "../gcc-${GCC_VER}/configure" \
        --target="$TARGET" --prefix="$PREFIX" \
        --disable-nls --enable-languages=c,c++ \
        --without-headers
    make -j$(nproc) all-gcc all-target-libgcc
    make install-gcc install-target-libgcc
    cd

    # PATH'e ekle
    echo "export PATH=\"$PREFIX/bin:\$PATH\"" >> ~/.bashrc
    export PATH="$PREFIX/bin:$PATH"
    echo "[OK] Cross-compiler kuruldu: $PREFIX/bin"
fi

# Doğrulama
echo ""
echo "=== Kurulum Doğrulama ==="
command -v x86_64-elf-gcc  &>/dev/null && echo "[OK] x86_64-elf-gcc"   || echo "[!!] x86_64-elf-gcc YOK"
command -v nasm             &>/dev/null && echo "[OK] nasm"             || echo "[!!] nasm YOK"
command -v qemu-system-x86_64 &>/dev/null && echo "[OK] qemu"          || echo "[!!] qemu YOK"
command -v grub-mkrescue    &>/dev/null && echo "[OK] grub-mkrescue"    || echo "[!!] grub-mkrescue YOK"
command -v xorriso          &>/dev/null && echo "[OK] xorriso"          || echo "[!!] xorriso YOK"

echo ""
echo "=== Kurulum Tamamlandı! ==="
echo "WSL2'den proje klasörüne git ve 'make' çalıştır:"
echo "  cd /mnt/d/Mevcut\\ Projeler/İşletim\\ Sistemi"
echo "  make"
echo "  make run"
