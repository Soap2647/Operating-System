// theme.hpp — MertOS Desktop renk paleti (Catppuccin Mocha)
// Windows 11 / GNOME hibrit düzen
#pragma once
#include <stdint.h>

namespace Theme {
    // ---- Catppuccin Mocha Palette ----
    static constexpr uint32_t Base      = 0x001E1E2E;
    static constexpr uint32_t Mantle    = 0x00181825;
    static constexpr uint32_t Crust     = 0x0011111B;
    static constexpr uint32_t Surface0  = 0x00313244;
    static constexpr uint32_t Surface1  = 0x0045475A;
    static constexpr uint32_t Surface2  = 0x00585B70;
    static constexpr uint32_t Overlay0  = 0x006C7086;
    static constexpr uint32_t Text      = 0x00CDD6F4;
    static constexpr uint32_t Subtext   = 0x00BAC2DE;
    static constexpr uint32_t Dim       = 0x006C7086;
    static constexpr uint32_t Blue      = 0x0089B4FA;
    static constexpr uint32_t Purple    = 0x00CBA6F7;
    static constexpr uint32_t Green     = 0x00A6E3A1;
    static constexpr uint32_t Yellow    = 0x00F9E2AF;
    static constexpr uint32_t Red       = 0x00F38BA8;
    static constexpr uint32_t Cyan      = 0x0089DCEB;
    static constexpr uint32_t Peach     = 0x00FAB387;
    static constexpr uint32_t Teal      = 0x0094E2D5;
    static constexpr uint32_t Mauve     = 0x00CBA6F7;

    // ---- Windows 11 tarzı düzen ----
    static constexpr int TASKBAR_H    = 48;   // Alt taskbar (Windows 11)
    static constexpr int STATUSBAR_H  = 0;    // Yok
    static constexpr int PANEL_H      = 0;    // Yok (taskbar altta)
    static constexpr int TITLEBAR_H   = 32;   // Pencere başlık çubuğu
    static constexpr int WIN_BORDER   = 1;    // İnce kenarlık
    static constexpr int WIN_MARGIN   = 16;   // Ekran kenarına mesafe
    static constexpr int BTN_RADIUS   = 6;    // Buton yarıçapı
    static constexpr int BTN_SPACING  = 18;   // Butonlar arası
    static constexpr int BTN_OFF_X    = 12;   // Sağ kenardan offset
}
