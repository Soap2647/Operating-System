# Custom Operating System Kernel

A custom-built operating system kernel written in C and Assembly. It features a custom bootloader setup, low-level memory management, and hardware communication protocols. This project demonstrates fundamental OS concepts, including interrupt handling and basic terminal output. A WSL setup script is included to help configure the cross-compiler toolchain required for building the `.iso` image.

## Features
- **Custom Bootloader:** Boots the kernel via GRUB.
- **Kernel:** Basic memory management and interrupt handlers.
- **VGA Text Mode:** Custom terminal driver for displaying text on screen.
- **Build System:** Makefile-based build process to generate bootable `.iso` files.

## Technologies Used
- **C & Assembly (x86)**
- **GCC Cross-Compiler** (i686-elf)
- **Make**
- **QEMU** (for virtualization)

## Setup and Execution
1. Set up the environment (WSL/Linux recommended):
   ```bash
   ./setup_wsl.sh
   ```
2. Build the OS image:
   ```bash
   make
   ```
3. Run in QEMU:
   ```bash
   make run
   ```

---

# Özel İşletim Sistemi Çekirdeği

C ve Assembly dilleri kullanılarak sıfırdan yazılmış özel bir işletim sistemi çekirdeğidir (kernel). Kendi bootloader (önyükleyici) yapılandırmasına, düşük seviyeli bellek yönetimine ve donanım iletişim protokollerine sahiptir. Kesme (interrupt) yönetimi ve temel terminal çıkışı gibi temel işletim sistemi kavramlarını uygulamalı olarak gösterir. `.iso` imajını derlemek için gereken çapraz derleyici (cross-compiler) ortamını kurmaya yardımcı olan bir WSL kurulum betiği içerir.

## Özellikler
- **Özel Bootloader:** Çekirdeği GRUB üzerinden önyükler (boot eder).
- **Çekirdek (Kernel):** Temel bellek yönetimi ve kesme (interrupt) işleyicileri.
- **VGA Metin Modu:** Ekranda metin görüntülemek için özel yazılmış terminal sürücüsü.
- **Derleme Sistemi:** Boot edilebilir `.iso` dosyaları oluşturmak için Makefile tabanlı derleme süreci.

## Kullanılan Teknolojiler
- **C & Assembly (x86)**
- **GCC Cross-Compiler** (i686-elf)
- **Make**
- **QEMU** (sanallaştırma için)

## Kurulum ve Çalıştırma
1. Geliştirme ortamını hazırlayın (WSL/Linux önerilir):
   ```bash
   ./setup_wsl.sh
   ```
2. İşletim sistemi imajını derleyin:
   ```bash
   make
   ```
3. QEMU üzerinde çalıştırın:
   ```bash
   make run
   ```
