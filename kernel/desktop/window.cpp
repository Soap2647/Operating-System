// window.cpp — Windows 11 tarzı pencere
#include "window.hpp"
#include "app.hpp"
#include "../../libc/include/kstring.hpp"

Window::Window()
{
    kstrncpy(m_title, "Window", 64);
}

void Window::setTitle(const char* title)
{
    kstrncpy(m_title, title, 63);
    m_title[63] = '\0';
}

void Window::draw(FramebufferDriver* fb)
{
    if (!m_visible || !fb) return;

    // Focused mu? AppManager'dan kontrol et
    bool focused = false;
    if (g_app_manager && g_app_manager->focused()) {
        focused = (g_app_manager->focused()->window() == this);
    }

    // ---- Gölge (shadow) — 6 katmanlı gradient, focused'da daha belirgin ----
    {
        static const uint8_t shadow_f[] = { 70, 52, 38, 26, 16, 8 };
        static const uint8_t shadow_u[] = { 45, 32, 22, 14, 8,  4 };
        const uint8_t* sa = focused ? shadow_f : shadow_u;
        for (int s = 5; s >= 0; s--) {
            fb->fillRectBlend(m_x - s + 4, m_y - s + 6,
                              m_w + s*2,   m_h + s*2,
                              Theme::Crust, sa[5 - s]);
        }
    }

    // ---- Dış kenarlık ----
    uint32_t border_color = focused ? Theme::Surface1 : Theme::Surface0;
    fb->drawRect(m_x, m_y, m_w, m_h, border_color);

    // ---- Title bar — focused/unfocused gradient ----
    int tbh = Theme::TITLEBAR_H;
    for (int i = 0; i < tbh; i++) {
        uint32_t from = focused ? Theme::Surface0 : Theme::Mantle;
        uint32_t to   = focused ? Theme::Mantle   : Theme::Crust;
        uint32_t row_color = FramebufferDriver::blendColor(from, to,
            (uint8_t)(200 - i * 100 / tbh));
        fb->drawHLine(m_x + 1, m_y + 1 + i, m_w - 2, row_color);
    }

    // Title bar alt ayırıcı
    uint32_t sep_color = focused ? Theme::Surface1 : Theme::Surface0;
    fb->drawHLine(m_x + 1, m_y + tbh, m_w - 2, sep_color);

    // ---- İçerik alanı ----
    fb->fillRect(contentX(), contentY(), contentW(), contentH(), Theme::Base);

    // ---- Title bar elementleri ----
    drawTitleBar(fb);
}

// ============================================================
// hitButton — Title bar buton hit-test
// Döner: -1=yok, 0=kapat, 1=küçült, 2=büyüt
// ============================================================
int Window::hitButton(int mx, int my) const
{
    if (!m_visible) return -1;
    // Title bar alanı dışındaysa yok
    if (my < m_y + 1 || my > m_y + Theme::TITLEBAR_H) return -1;
    if (mx < m_x     || mx > m_x + m_w)                return -1;

    int by = m_y + 1 + (Theme::TITLEBAR_H - 1) / 2;
    int bx = m_x + m_w - Theme::BTN_OFF_X;
    int r  = Theme::BTN_RADIUS + 2;  // Tıklama alanı biraz geniş

    // Kapat — en sağ (kırmızı)
    if (mx >= bx - r && mx <= bx + r && my >= by - r && my <= by + r) return 0;
    // Küçült — ortadaki (sarı)
    bx -= Theme::BTN_SPACING;
    if (mx >= bx - r && mx <= bx + r && my >= by - r && my <= by + r) return 1;
    // Büyüt — en sol (yeşil)
    bx -= Theme::BTN_SPACING;
    if (mx >= bx - r && mx <= bx + r && my >= by - r && my <= by + r) return 2;

    return -1;
}

void Window::drawTitleBar(FramebufferDriver* fb)
{
    // Başlık metni
    drawTitleText(fb);

    // Butonlar SAĞDA (Windows 11 / KDE tarzı)
    int by = m_y + 1 + (Theme::TITLEBAR_H - 1) / 2;
    int bx = m_x + m_w - Theme::BTN_OFF_X;

    // ● Kapat (Red)
    drawButton(fb, bx, by, Theme::Red);
    // ● Küçült (Yellow)
    drawButton(fb, bx - Theme::BTN_SPACING, by, Theme::Yellow);
    // ● Büyüt (Green)
    drawButton(fb, bx - Theme::BTN_SPACING * 2, by, Theme::Green);

    // ---- Sol: App ikonu — AppManager'dan app rengi ve sembolü al ----
    {
        uint32_t app_color = Theme::Blue;
        char     app_sym   = ' ';
        if (g_app_manager) {
            for (int ai = 0; ai < g_app_manager->count(); ai++) {
                App* ap = g_app_manager->get(ai);
                if (ap && ap->window() == this) {
                    app_color = ap->getColor();
                    app_sym   = ap->getSymbol();
                    break;
                }
            }
        }
        int ix = m_x + 8;
        int iy = m_y + 1 + (Theme::TITLEBAR_H - 1 - 16) / 2;
        // 16x16 yuvarlatılmış ikon
        int r = 3;
        fb->fillRect(ix + r, iy,     16 - 2*r, 16,       app_color);
        fb->fillRect(ix,     iy + r, 16,        16 - 2*r, app_color);
        fb->fillRect(ix + r, iy + r, 16 - 2*r,  16 - 2*r, app_color);
        // Sembol
        char sym[2] = { app_sym, '\0' };
        fb->drawString(ix + 4, iy + 4, sym, Theme::Base, app_color);
    }
}

void Window::drawButton(FramebufferDriver* fb, int cx, int cy, uint32_t color)
{
    int r = Theme::BTN_RADIUS;
    fb->fillRect(cx - r + 2, cy - r,     2*r - 3, 2*r + 1, color);
    fb->fillRect(cx - r,     cy - r + 2, 2*r + 1, 2*r - 3, color);
    fb->fillRect(cx - r + 1, cy - r + 1, 2*r - 1, 2*r - 1, color);
    // Highlight (üst sol köşe)
    fb->putPixel(cx - r + 2, cy - r + 1,
        FramebufferDriver::blendColor(0x00FFFFFF, color, 60));
}

void Window::drawTitleText(FramebufferDriver* fb)
{
    int title_len = kstrlen(m_title);
    int text_w    = title_len * FramebufferDriver::FONT_W;
    // Başlığı ortala, ama app ikonun sağına kaydır
    int tx = m_x + m_w / 2 - text_w / 2;
    int ty = m_y + 1 + (Theme::TITLEBAR_H - 1 - FramebufferDriver::FONT_H) / 2;

    fb->drawString(tx, ty, m_title, Theme::Subtext, Theme::Surface0);
}
