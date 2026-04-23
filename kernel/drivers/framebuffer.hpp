// framebuffer.hpp — VESA/GOP Framebuffer Driver
// OOP: FramebufferDriver : public Driver
// Piksel bazlı çizim, 8x8 builtin font ile text render

#pragma once
#include "driver.hpp"
#include "../memory/multiboot2.hpp"

// ============================================================
// 32-bit ARGB renk yardımcıları
// ============================================================
namespace Color {
    static constexpr uint32_t Black   = 0x00000000;
    static constexpr uint32_t White   = 0x00FFFFFF;
    static constexpr uint32_t Red     = 0x00FF0000;
    static constexpr uint32_t Green   = 0x0000FF00;
    static constexpr uint32_t Blue    = 0x000000FF;
    static constexpr uint32_t Cyan    = 0x0000FFFF;
    static constexpr uint32_t Magenta = 0x00FF00FF;
    static constexpr uint32_t Yellow  = 0x00FFFF00;
    static constexpr uint32_t Gray    = 0x00AAAAAA;
    static constexpr uint32_t DarkGray= 0x00444444;
    static constexpr uint32_t Orange  = 0x00FF8800;

    // VGA renk indexini 32-bit RGB'ye çevir
    inline uint32_t fromVGA(uint8_t idx) {
        static const uint32_t table[16] = {
            0x000000, 0x0000AA, 0x00AA00, 0x00AAAA,
            0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
            0x555555, 0x5555FF, 0x55FF55, 0x55FFFF,
            0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF
        };
        return (idx < 16) ? table[idx] : 0;
    }
}

// ============================================================
// FramebufferDriver
// ============================================================
class FramebufferDriver : public Driver {
public:
    static constexpr int FONT_W = 8;
    static constexpr int FONT_H = 8;

    FramebufferDriver() : Driver(Type::Display, 10) {}

    // Driver interface
    bool        initialize() override;
    const char* getName()    const override { return "VESA Framebuffer Driver"; }

    // Framebuffer'ı multiboot2 tag'inden başlat
    bool initFromTag(const Mb2TagFramebuffer* tag);

    // Bochs VBE Extension (BGA) doğrudan init — GRUB vermezse kernel ayarlar
    // QEMU -vga std, VMware SVGA, VirtualBox ile çalışır
    bool initFromBochsVBE(int w = 1024, int h = 768);

    // Temel çizim
    void     putPixel(int x, int y, uint32_t color);
    uint32_t getPixel(int x, int y) const;          // Cursor için arka plan kaydet
    void fillRect(int x, int y, int w, int h, uint32_t color);
    void drawChar(int x, int y, char c, uint32_t fg, uint32_t bg);
    void drawString(int x, int y, const char* s, uint32_t fg, uint32_t bg);
    void clear(uint32_t color = Color::Black);
    void drawHLine(int x, int y, int w, uint32_t color);
    void drawVLine(int x, int y, int h, uint32_t color);
    void drawRect(int x, int y, int w, int h, uint32_t color);

    void drawCharScaled(int x, int y, char c, uint32_t fg, uint32_t bg, int scale);
    void drawStringScaled(int x, int y, const char* s, uint32_t fg, uint32_t bg, int scale);
    void fillRectBlend(int x, int y, int w, int h, uint32_t color, uint8_t alpha);
    static uint32_t blendColor(uint32_t fg, uint32_t bg, uint8_t alpha);
    void setFontScale(int s) { m_scale = s; }
    int  fontScale() const   { return m_scale; }

    // Text console (satır/sütun bazlı, shell için)
    void consolePutChar(char c, uint32_t fg = Color::White, uint32_t bg = Color::Black);
    void consolePrint(const char* s, uint32_t fg = Color::White, uint32_t bg = Color::Black);
    void consolePrintColored(const char* s, uint32_t fg, uint32_t bg = Color::Black);
    void consoleClear();
    void consoleScroll();
    void consoleNewLine();
    void consoleSetCursor(int col, int row);
    void consoleGetCursor(int& col, int& row) const { col = m_col; row = m_row; }

    // Console pencere sınırı (Desktop entegrasyonu için)
    void consoleSetBounds(int x, int y, int w, int h) {
        m_con_x = x; m_con_y = y; m_con_w = w; m_con_h = h;
        m_col = 0; m_row = 0;
    }

    // Boyut sorgulama
    int width()  const { return m_width; }
    int height() const { return m_height; }
    int cols()   const { return (m_con_w > 0 ? m_con_w : m_width)  / (FONT_W * m_scale); }
    int rows()   const { return (m_con_h > 0 ? m_con_h : m_height) / (FONT_H * m_scale); }
    bool isReady() const { return m_fb != nullptr; }

private:
    uint32_t* m_fb     = nullptr;  // Framebuffer pointer
    int       m_width  = 0;
    int       m_height = 0;
    int       m_pitch  = 0;        // Byte per row
    int       m_col    = 0;        // Text cursor column
    int       m_row    = 0;        // Text cursor row
    // Console bounds (desktop pencere içeriği için)
    int       m_con_x  = 0;        // Text origin X (piksel)
    int       m_con_y  = 0;        // Text origin Y (piksel)
    int       m_con_w  = 0;        // Text area width  (0 = tam ekran)
    int       m_con_h  = 0;        // Text area height (0 = tam ekran)

    int       m_scale  = 1;        // Font scale (1=8x8, 2=16x16)

    // Builtin 8x8 font (CP437 subset, ASCII 32-127)
    static const uint8_t s_font[96][8];
};

extern FramebufferDriver* g_fb;
