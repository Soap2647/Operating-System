// splash.cpp — Boot splash screen implementasyonu
// SplashScreen : Widget (Inheritance)
#include "splash.hpp"
#include "../drivers/framebuffer.hpp"

// Timer tick sayacı (kernel.cpp'de tanımlanır)
extern uint64_t g_timer_ticks;

void SplashScreen::waitTick()
{
    volatile uint64_t t = g_timer_ticks;
    while (g_timer_ticks == t) asm volatile("hlt");
}

// ============================================================
// draw() — tek kare çiz (animate() içinde kullanılır)
// ============================================================
void SplashScreen::draw(FramebufferDriver* fb)
{
    if (!fb) return;
    int W = fb->width();
    int H = fb->height();

    // Arka plan
    fb->fillRect(0, 0, W, H, Theme::Crust);

    // Logo + başlık
    drawLogo(fb, W/2, H/2 - 60);

    // Alt yazı
    const char* sub = "Kastamonu Universitesi - BIL Bolumu";
    int sw = 35 * 8;
    fb->drawString(W/2 - sw/2, H/2 + 60, sub, Theme::Dim, Theme::Crust);
}

// ============================================================
// drawLogo — MertOS "M" logosu + başlık
// ============================================================
void SplashScreen::drawLogo(FramebufferDriver* fb, int cx, int cy)
{
    // 64x64 logo bloğu (Purple → Blue degrade efekti — fillRect katmanlarıyla)
    int lx = cx - 32, ly = cy - 32;
    fb->fillRect(lx,      ly,      64, 64, Theme::Purple);
    fb->fillRect(lx+2,    ly+2,    60, 60, Theme::Mantle);
    fb->fillRect(lx+8,    ly+8,    48, 48, Theme::Purple);
    fb->fillRect(lx+14,   ly+14,   36, 36, Theme::Blue);
    fb->fillRect(lx+20,   ly+20,   24, 24, Theme::Purple);
    // Köşe vurguları
    fb->fillRect(lx+4,    ly+4,    8,  8,  Theme::Blue);
    fb->fillRect(lx+52,   ly+4,    8,  8,  Theme::Blue);
    fb->fillRect(lx+4,    ly+52,   8,  8,  Theme::Teal);
    fb->fillRect(lx+52,   ly+52,   8,  8,  Theme::Teal);

    // "MertOS" başlık (2x büyük)
    const char* title = "MertOS";
    int tw = 6 * 8 * 2;
    fb->drawStringScaled(cx - tw/2, cy + 40, title, Theme::Text, Theme::Crust, 2);

    // "v0.3.0 — x86_64" versiyon
    const char* ver = "v0.3.0  x86_64";
    int vw = 14 * 8;
    fb->drawString(cx - vw/2, cy + 72, ver, Theme::Subtext, Theme::Crust);
}

// ============================================================
// drawLoadingBar — ilerleme çubuğu
// ============================================================
void SplashScreen::drawLoadingBar(FramebufferDriver* fb, int cx, int cy, int progress, int total)
{
    int bw = fb->width() / 3;
    int bh = 4;
    int bx = cx - bw/2;

    // Arka plan (boş bar)
    fb->fillRect(bx - 1, cy - 1, bw + 2, bh + 2, Theme::Surface0);
    fb->fillRect(bx, cy, bw, bh, Theme::Surface1);

    // Dolmuş kısım
    int filled = bw * progress / total;
    if (filled > 0) {
        // Gradient efekti: Purple → Blue
        int half = filled / 2;
        fb->fillRect(bx,        cy, half,          bh, Theme::Purple);
        fb->fillRect(bx + half, cy, filled - half, bh, Theme::Blue);
    }
}

// ============================================================
// animate() — 2 saniyelik animasyonlu açılış
// ============================================================
void SplashScreen::animate(FramebufferDriver* fb)
{
    if (!fb || !fb->isReady()) return;

    int W = fb->width();
    int H = fb->height();
    int barY = H/2 + 100;

    // Statik içeriği çiz
    draw(fb);

    // Loading bar animasyonu (~36 tick @ 18.2Hz ≈ 2 saniye)
    const int FRAMES = 36;
    for (int i = 0; i <= FRAMES; i++) {
        drawLoadingBar(fb, W/2, barY, i, FRAMES);

        // Yüzde yazısı
        char pct[5];
        int p = i * 100 / FRAMES;
        pct[0] = '0' + (p/100);
        pct[1] = '0' + (p/10)%10;
        pct[2] = '0' + p%10;
        pct[3] = '%';
        pct[4] = '\0';
        // Öncekini sil
        fb->fillRect(W/2 - 20, barY + 10, 40, 8, Theme::Crust);
        fb->drawString(W/2 - 16, barY + 10, pct, Theme::Subtext, Theme::Crust);

        waitTick();
    }

    // Kısa bekleme
    for (int i = 0; i < 5; i++) waitTick();

    // Fade out (ekranı yavaşça kararalt)
    fadeOut(fb);
}

// ============================================================
// fadeOut — 16 adımda ekranı kararalt
// ============================================================
void SplashScreen::fadeOut(FramebufferDriver* fb)
{
    int W = fb->width();
    int H = fb->height();

    for (int step = 0; step < 16; step++) {
        // Her adımda ekranı %6 daha fazla kararalt
        fb->fillRectBlend(0, 0, W, H, 0x00000000, (uint8_t)(step * 16));
        waitTick();
    }
    // Tamamen siyah
    fb->fillRect(0, 0, W, H, 0x00000000);
}
