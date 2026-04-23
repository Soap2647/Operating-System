// desktop.cpp — MertOS Desktop (yeniden tasarlandı)
// OOP: Desktop : public Widget (INHERITANCE)
//      Desktop HAS-A DesktopIcon[] (COMPOSITION)
//      AppManager ile çalışır
#include "desktop.hpp"
#include "../../libc/include/kstring.hpp"

extern uint64_t g_timer_ticks;

Desktop* g_desktop = nullptr;

// Arrow cursor bitmap (20 satır, 12 bit genişlik)
static const uint16_t s_cursor[20] = {
    0b1000000000000000,
    0b1100000000000000,
    0b1110000000000000,
    0b1111000000000000,
    0b1111100000000000,
    0b1111110000000000,
    0b1111111000000000,
    0b1111111100000000,
    0b1111111110000000,
    0b1111110000000000,
    0b1101100000000000,
    0b1000110000000000,
    0b0000110000000000,
    0b0000011000000000,
    0b0000011000000000,
    0b0000001100000000,
    0b0000001100000000,
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000,
};

// ============================================================
// Constructor
// ============================================================
Desktop::Desktop()
    : m_icon_count(0)
    , m_icon_hover(-1)
    , m_drag_app(-1), m_drag_start_x(0), m_drag_start_y(0)
    , m_drag_win_start_x(0), m_drag_win_start_y(0)
    , m_last_click_time(0), m_last_click_app(-1)
    , m_dock_hover(-1)
    , m_cur_x(512), m_cur_y(384), m_cursor_drawn(false)
{
    for (int i = 0; i < 8; i++) {
        kmemset(&m_icons[i], 0, sizeof(DesktopIcon));
        m_icons[i].app_idx = -1;
        m_saved_state[i] = { 0, 0, 0, 0, false };
    }
    for (int i = 0; i < CUR_W * CUR_H; i++) m_cursor_bg[i] = 0;
}

// ============================================================
// addIcon — Masaüstü ikonu ekle
// ============================================================
void Desktop::addIcon(const char* label, uint32_t color, char symbol, int app_idx)
{
    if (m_icon_count >= 8) return;
    DesktopIcon& icon = m_icons[m_icon_count];
    kstrncpy(icon.label, label, 31);
    icon.label[31] = '\0';
    icon.color     = color;
    icon.symbol    = symbol;
    icon.app_idx   = app_idx;
    // Konum: masaüstünde 80px aralıklı, sol üst köşeye hizalı
    icon.x = 20 + (m_icon_count % 2) * 90;
    icon.y = TOPBAR_H + 20 + (m_icon_count / 2) * 90;
    m_icon_count++;
}

// ============================================================
// draw — Ana çizim: arka plan, uygulamalar, topbar, dock
// ============================================================
void Desktop::draw(FramebufferDriver* fb)
{
    if (!fb) return;

    drawBackground(fb);
    drawDesktopArea(fb);

    if (g_app_manager) g_app_manager->drawAll(fb);

    drawTopBar(fb);
    drawDock(fb);
}

// ============================================================
// drawBackground — Subtle gradient arka plan
// ============================================================
void Desktop::drawBackground(FramebufferDriver* fb)
{
    int W = fb->width();
    int H = fb->height();
    fb->fillRect(0, 0, W, H, Theme::Base);

    // Subtle dot grid — her 24px bir nokta (Catppuccin Surface0 rengi)
    for (int y = TOPBAR_H + 12; y < H - DOCK_H; y += 24) {
        for (int x = 12; x < W; x += 24) {
            fb->putPixel(x, y, Theme::Surface0);
        }
    }

    // Üst gradient (Mantle'dan şeffafa, TOPBAR'ın hemen altı)
    for (int i = 0; i < 60; i++) {
        uint8_t alpha = static_cast<uint8_t>(25 - i * 25 / 60);
        if (alpha > 0)
            fb->fillRectBlend(0, TOPBAR_H + i, W, 1, Theme::Mantle, alpha);
    }
}

// ============================================================
// drawDesktopArea — Masaüstü ikonları
// ============================================================
void Desktop::drawDesktopArea(FramebufferDriver* fb)
{
    for (int i = 0; i < m_icon_count; i++) {
        DesktopIcon& icon = m_icons[i];
        int ix = icon.x;
        int iy = icon.y;
        bool hovered = (m_icon_hover == i);

        // Hover: hafif beyaz overlay arka plan
        if (hovered) {
            fb->fillRectBlend(ix - 4, iy - 4, 56, 56, 0x00FFFFFF, 18);
        }

        // 48x48 "yuvarlatılmış" rect (3 örtüşen fillRect ile)
        int r = 8;
        fb->fillRect(ix + r,     iy,          48 - 2*r, 48,          icon.color);
        fb->fillRect(ix,         iy + r,      48,        48 - 2*r,    icon.color);
        fb->fillRect(ix + r,     iy + r,      48 - 2*r,  48 - 2*r,   icon.color);

        // Hover'da biraz daha parlak overlay
        if (hovered) {
            fb->fillRectBlend(ix, iy, 48, 48, 0x00FFFFFF, 30);
        }

        // Sembol karakteri (ortada)
        char sym[2] = { icon.symbol, '\0' };
        fb->drawString(ix + 20, iy + 20, sym, Theme::Base, icon.color);

        // Etiket (ikon altında) — hover'da altı çizgili efekt
        fb->drawString(ix + 2, iy + 52, icon.label, hovered ? Theme::Text : Theme::Subtext, Theme::Base);
        if (hovered) {
            int llen = kstrlen(icon.label);
            fb->fillRect(ix + 2, iy + 61, llen * 8, 1, Theme::Subtext);
        }
    }
}

// ============================================================
// drawTopBar — Üst bilgi çubuğu (y=0, h=TOPBAR_H)
// ============================================================
void Desktop::drawTopBar(FramebufferDriver* fb)
{
    int W = fb->width();

    fb->fillRect(0, 0, W, TOPBAR_H, Theme::Mantle);
    fb->drawHLine(0, TOPBAR_H - 1, W, Theme::Surface0);

    // ---- Sol: MertOS logosu (küçük 20x20 iç içe rect) ----
    int lx = 10;
    int ly = (TOPBAR_H - 20) / 2;
    fb->fillRect(lx,      ly,      20, 20, Theme::Purple);
    fb->fillRect(lx + 2,  ly + 2,  16, 16, Theme::Mantle);
    fb->fillRect(lx + 5,  ly + 5,  10, 10, Theme::Purple);
    fb->fillRect(lx + 8,  ly + 8,   4,  4, Theme::Blue);

    // "MertOS" metni
    fb->drawString(36, (TOPBAR_H - 8) / 2, "MertOS", Theme::Text, Theme::Mantle);

    // ---- Orta: Aktif pencere başlığı ----
    if (g_app_manager && g_app_manager->focused()) {
        App* focused_app = g_app_manager->focused();
        if (focused_app->window() && focused_app->window()->visible()) {
            const char* title = focused_app->getName();
            int tlen = kstrlen(title);
            int title_x = (W - tlen * 8) / 2;
            fb->drawString(title_x, (TOPBAR_H - 8) / 2, title, Theme::Text, Theme::Mantle);
        }
    }

    // ---- Orta-Sol: Açık uygulamalar için pill butonlar ----
    if (g_app_manager) {
        int visible_count = 0;
        for (int i = 0; i < g_app_manager->count(); i++) {
            App* app = g_app_manager->get(i);
            if (app && app->window() && app->window()->visible()) visible_count++;
        }
        int pill_start_x = 90;
        int pill_idx = 0;
        for (int i = 0; i < g_app_manager->count(); i++) {
            App* app = g_app_manager->get(i);
            if (!app) continue;
            bool visible = app->window() && app->window()->visible();
            if (!visible) continue;

            int pill_x = pill_start_x + pill_idx * 74;
            int pill_y = (TOPBAR_H - 20) / 2;
            bool focused = (g_app_manager->focused() == app);

            uint32_t pill_bg = focused ? Theme::Surface1 : Theme::Surface0;
            // Yuvarlatılmış pill (3 fillRect)
            fb->fillRect(pill_x + 4, pill_y,      62, 20, pill_bg);
            fb->fillRect(pill_x,     pill_y + 4,  70, 12, pill_bg);
            fb->fillRect(pill_x + 4, pill_y + 4,  62, 12, pill_bg);

            if (focused) {
                fb->fillRect(pill_x + 4, pill_y + 18, 62, 2, Theme::Blue);
            }

            // Uygulama adı
            const char* name = app->getName();
            int nlen = kstrlen(name);
            int text_x = pill_x + (70 - nlen * 8) / 2;
            if (text_x < pill_x) text_x = pill_x + 2;
            fb->drawString(text_x, pill_y + 6, name, focused ? Theme::Text : Theme::Subtext, pill_bg);
            pill_idx++;
        }
    }

    // ---- Sağ: HH:MM:SS saat ----
    {
        uint64_t total_s = g_timer_ticks / 18;
        uint64_t h  = total_s / 3600;
        uint64_t m  = (total_s % 3600) / 60;
        uint64_t s  = total_s % 60;

        // 2 haneli sıfır-dolgulu format yardımcısı (inline)
        auto pad2 = [](char* dst, uint64_t v) {
            dst[0] = '0' + (char)(v / 10);
            dst[1] = '0' + (char)(v % 10);
            dst[2] = '\0';
        };

        char timebuf[10];
        char tmp[4];
        timebuf[0] = '\0';
        pad2(tmp, h);  kstrcat(timebuf, tmp);
        kstrcat(timebuf, ":");
        pad2(tmp, m);  kstrcat(timebuf, tmp);
        kstrcat(timebuf, ":");
        pad2(tmp, s);  kstrcat(timebuf, tmp);

        int tlen = kstrlen(timebuf);
        fb->drawString(W - 8 - tlen * 8, (TOPBAR_H - 8) / 2, timebuf, Theme::Dim, Theme::Mantle);
    }
}

// ============================================================
// drawDock — Alt dock (y=H-DOCK_H, h=DOCK_H)
// 4 sabit ikon: Terminal, Dosyalar, Ayarlar, MSC
// ============================================================
void Desktop::drawDock(FramebufferDriver* fb)
{
    int W  = fb->width();
    int H  = fb->height();
    int DY = H - DOCK_H;

    fb->fillRect(0, DY, W, DOCK_H, Theme::Mantle);
    fb->drawHLine(0, DY, W, Theme::Surface0);

    // Dock ikon özellikleri
    struct DockItem {
        const char* label;
        uint32_t    color;
        char        symbol;
    };
    static const DockItem items[4] = {
        { "Terminal", Theme::Mauve,  'T' },
        { "Dosyalar", Theme::Blue,   'F' },
        { "Ayarlar",  Theme::Teal,   'S' },
        { "MSC",      Theme::Peach,  'M' },
    };

    // 4 ikon, ortalanmış
    int icon_w   = 44;
    int icon_gap = 8;
    int total_w  = 4 * icon_w + 3 * icon_gap;
    int start_x  = (W - total_w) / 2;
    int icon_y   = DY + (DOCK_H - icon_w) / 2;

    for (int i = 0; i < 4; i++) {
        int ix = start_x + i * (icon_w + icon_gap);
        bool hovered = (m_dock_hover == i);

        // Hover: arka plan highlight
        if (hovered) {
            fb->fillRectBlend(ix - 3, icon_y - 3, icon_w + 6, icon_w + 6, 0x00FFFFFF, 15);
        }

        // "Yuvarlatılmış rect" — 3 örtüşen fillRect
        int r = 6;
        fb->fillRect(ix + r,    icon_y,       icon_w - 2*r, icon_w,       items[i].color);
        fb->fillRect(ix,        icon_y + r,   icon_w,        icon_w - 2*r, items[i].color);
        fb->fillRect(ix + r,    icon_y + r,   icon_w - 2*r,  icon_w - 2*r, items[i].color);

        // Hover'da parlak overlay
        if (hovered) {
            fb->fillRectBlend(ix, icon_y, icon_w, icon_w, 0x00FFFFFF, 35);
        }

        // Sembol
        char sym[2] = { items[i].symbol, '\0' };
        fb->drawString(ix + (icon_w - 8) / 2, icon_y + (icon_w - 8) / 2,
                       sym, Theme::Base, items[i].color);

        // Açık/focused göstergesi
        if (g_app_manager) {
            App* app = g_app_manager->get(i);
            if (app && app->isOpen() && app->window() && app->window()->visible()) {
                bool is_focused = (g_app_manager->focused() == app);
                if (is_focused) {
                    // Focused: renkli 4px alt çizgi + hafif Surface0 overlay
                    fb->fillRect(ix, DY + DOCK_H - 5, icon_w, 4, items[i].color);
                    fb->fillRectBlend(ix, icon_y, icon_w, icon_w, 0x00FFFFFF, 20);
                } else {
                    // Açık ama focused değil: 3px beyaz nokta
                    int dot_x = ix + icon_w / 2 - 1;
                    int dot_y = DY + DOCK_H - 6;
                    fb->fillRect(dot_x, dot_y, 3, 3, Theme::Subtext);
                }
            }
        }

        // Hover: uygulama adı tooltip (dock üstünde küçük etiket)
        if (hovered) {
            int llen = kstrlen(items[i].label);
            int lx   = ix + (icon_w - llen * 8) / 2;
            int ly   = icon_y - 16;
            fb->fillRect(lx - 3, ly - 2, llen * 8 + 6, 12, Theme::Surface1);
            fb->drawString(lx, ly, items[i].label, Theme::Text, Theme::Surface1);
        }
    }
}

// ============================================================
// handleClick — Tıklama yönetimi
// OOP: App* üzerinden onMouse() dispatch → Polymorphism
// ============================================================
void Desktop::handleClick(int mx, int my)
{
    int W  = m_w;
    int H  = m_h;
    int DY = H - DOCK_H;

    // ---- Dock tıklaması ----
    if (my >= DY && my < H) {
        int icon_w   = 44;
        int icon_gap = 8;
        int total_w  = 4 * icon_w + 3 * icon_gap;
        int start_x  = (W - total_w) / 2;

        for (int i = 0; i < 4; i++) {
            int ix     = start_x + i * (icon_w + icon_gap);
            int icon_y = DY + (DOCK_H - icon_w) / 2;

            if (mx >= ix && mx < ix + icon_w && my >= icon_y && my < icon_y + icon_w) {
                if (!g_app_manager) return;
                App* app = g_app_manager->get(i);
                if (!app) return;

                Window* win = app->window();
                if (win) {
                    if (win->visible()) {
                        if (g_app_manager->focused() == app) {
                            // Açık + focused → minimize
                            win->setVisible(false);
                        } else {
                            // Açık ama unfocused → öne getir + focus
                            g_app_manager->bringToFront(i);
                        }
                    } else {
                        // Kapalı → aç + focus
                        win->setVisible(true);
                        g_app_manager->bringToFront(i);
                    }
                }
                if (g_fb) { m_cursor_drawn = false; draw(g_fb); }
                return;
            }
        }
        return;
    }

    // ---- Title bar tıklaması → buton veya drag ----
    if (g_app_manager) {
        for (int i = g_app_manager->count() - 1; i >= 0; i--) {
            App* app = g_app_manager->get(i);
            if (!app || !app->isOpen()) continue;
            Window* win = app->window();
            if (!win || !win->visible()) continue;

            int wx = win->x(), wy = win->y();
            int ww = win->width();

            // Title bar alanı?
            if (mx >= wx && mx < wx + ww &&
                my >= wy && my < wy + Theme::TITLEBAR_H) {

                // Önce buton hit-test yap
                int btn = win->hitButton(mx, my);
                if (btn == 0) {
                    // Kapat (kırmızı)
                    win->setVisible(false);
                    if (g_fb) draw(g_fb);
                    return;
                } else if (btn == 1) {
                    // Küçült (sarı)
                    win->setVisible(false);
                    if (g_fb) draw(g_fb);
                    return;
                } else if (btn == 2) {
                    // Büyüt (yeşil) — içerik alanını doldur
                    win->setPos(0, TOPBAR_H);
                    win->setSize(m_w, m_h - TOPBAR_H - DOCK_H);
                    g_app_manager->bringToFront(i);
                    if (g_fb) draw(g_fb);
                    return;
                }

                // Buton değil → çift tıklama kontrolü (maximize/restore)
                g_app_manager->bringToFront(i);
                int fi = g_app_manager->count() - 1; // bringToFront sonrası

                uint64_t now = g_timer_ticks;
                bool double_click = (m_last_click_app == fi) &&
                                    (now - m_last_click_time < 7); // ~400ms @ 18Hz
                m_last_click_time = now;
                m_last_click_app  = fi;

                if (double_click) {
                    WinState& ws = m_saved_state[i < 8 ? i : 0];
                    if (!ws.saved) {
                        // Maximize: kaydet ve tam ekran yap
                        ws = { win->x(), win->y(), win->width(), win->height(), true };
                        win->setPos(0, TOPBAR_H);
                        win->setSize(m_w, m_h - TOPBAR_H - DOCK_H);
                    } else {
                        // Restore: kayıtlı konuma dön
                        win->setPos(ws.x, ws.y);
                        win->setSize(ws.w, ws.h);
                        ws.saved = false;
                    }
                    if (g_fb) { m_cursor_drawn = false; draw(g_fb); }
                    return;
                }

                // Tek tıklama → drag başlat
                m_drag_app         = fi;
                m_drag_start_x     = mx;
                m_drag_start_y     = my;
                m_drag_win_start_x = wx;
                m_drag_win_start_y = wy;
                if (g_fb) draw(g_fb);
                return;
            }

            // Pencere içi tıklama (title bar dışı) → focus + onMouse
            if (mx >= wx && mx < wx + ww &&
                my >= wy + Theme::TITLEBAR_H && my < wy + win->height()) {
                g_app_manager->bringToFront(i);
                MouseEvent me;
                me.x = mx; me.y = my; me.buttons = 1; me.clicked = true;
                app->onMouse(me);   // Polymorphic dispatch
                if (g_fb) draw(g_fb);
                return;
            }
        }
    }

    // ---- Masaüstü ikonu tıklaması ----
    if (my >= TOPBAR_H && my < DY) {
        for (int i = 0; i < m_icon_count; i++) {
            DesktopIcon& icon = m_icons[i];
            if (mx >= icon.x && mx < icon.x + 48 &&
                my >= icon.y && my < icon.y + 48) {
                if (g_app_manager && icon.app_idx >= 0) {
                    App* app = g_app_manager->get(icon.app_idx);
                    if (app) {
                        if (app->window()) app->window()->setVisible(true);
                        g_app_manager->setFocus(icon.app_idx);
                        if (g_fb) draw(g_fb);
                    }
                }
                return;
            }
        }
    }
}

// ============================================================
// handleMouseMove — Drag sırasında pencereyi taşı
// ============================================================
void Desktop::handleMouseMove(int mx, int my)
{
    // ---- Dock hover güncelle ----
    {
        int old_hover = m_dock_hover;
        m_dock_hover = -1;
        int icon_w   = 44;
        int icon_gap = 8;
        int total_w  = 4 * icon_w + 3 * icon_gap;
        int start_x  = (m_w - total_w) / 2;
        int DY       = m_h - DOCK_H;
        int icon_y   = DY + (DOCK_H - icon_w) / 2;
        if (my >= DY && my < m_h) {
            for (int i = 0; i < 4; i++) {
                int ix = start_x + i * (icon_w + icon_gap);
                if (mx >= ix && mx < ix + icon_w && my >= icon_y && my < icon_y + icon_w) {
                    m_dock_hover = i;
                    break;
                }
            }
        }
        if (m_dock_hover != old_hover && g_fb) {
            m_cursor_drawn = false;
            draw(g_fb);
        }
    }

    // ---- Desktop ikon hover güncelle ----
    {
        int old_hover = m_icon_hover;
        m_icon_hover = -1;
        int DY = m_h - DOCK_H;
        if (my >= TOPBAR_H && my < DY) {
            for (int i = 0; i < m_icon_count; i++) {
                DesktopIcon& icon = m_icons[i];
                if (mx >= icon.x && mx < icon.x + 48 &&
                    my >= icon.y && my < icon.y + 48) {
                    m_icon_hover = i;
                    break;
                }
            }
        }
        if (m_icon_hover != old_hover && g_fb) {
            m_cursor_drawn = false;
            draw(g_fb);
        }
    }

    if (m_drag_app < 0 || !g_app_manager) return;
    App* app = g_app_manager->get(m_drag_app);
    if (!app || !app->window()) { m_drag_app = -1; return; }

    Window* win = app->window();
    int dx = mx - m_drag_start_x;
    int dy = my - m_drag_start_y;

    int new_x = m_drag_win_start_x + dx;
    int new_y = m_drag_win_start_y + dy;

    // Ekran sınırları içinde tut
    int H = m_h;
    if (new_x < 0)                          new_x = 0;
    if (new_y < TOPBAR_H)                   new_y = TOPBAR_H;
    if (new_x + win->width()  > m_w)        new_x = m_w  - win->width();
    if (new_y + win->height() > H - DOCK_H) new_y = H - DOCK_H - win->height();

    win->setPos(new_x, new_y);

    if (g_fb) {
        m_cursor_drawn = false;  // Çizimden önce cursor state'i sıfırla
        draw(g_fb);
    }
}

// ============================================================
// handleMouseRelease — Drag sona erdi
// ============================================================
void Desktop::handleMouseRelease(int mx, int my)
{
    (void)mx; (void)my;
    m_drag_app = -1;
}

// ============================================================
// Cursor save/restore/draw
// ============================================================
void Desktop::saveCursorBg(FramebufferDriver* fb, int x, int y)
{
    for (int row = 0; row < CUR_H; row++)
        for (int col = 0; col < CUR_W; col++)
            m_cursor_bg[row * CUR_W + col] = fb->getPixel(x + col, y + row);
}

void Desktop::restoreCursorBg(FramebufferDriver* fb, int x, int y)
{
    for (int row = 0; row < CUR_H; row++)
        for (int col = 0; col < CUR_W; col++)
            fb->putPixel(x + col, y + row, m_cursor_bg[row * CUR_W + col]);
}

void Desktop::drawCursorAt(FramebufferDriver* fb, int x, int y)
{
    // Siyah outline
    for (int row = 0; row < CUR_H; row++) {
        uint16_t bits = s_cursor[row];
        for (int col = 0; col < CUR_W; col++) {
            if ((bits >> (15 - col)) & 1) {
                fb->putPixel(x + col - 1, y + row,   0x00000000);
                fb->putPixel(x + col + 1, y + row,   0x00000000);
                fb->putPixel(x + col,     y + row - 1, 0x00000000);
                fb->putPixel(x + col,     y + row + 1, 0x00000000);
            }
        }
    }
    // Beyaz ok
    for (int row = 0; row < CUR_H; row++) {
        uint16_t bits = s_cursor[row];
        for (int col = 0; col < CUR_W; col++) {
            if ((bits >> (15 - col)) & 1)
                fb->putPixel(x + col, y + row, 0x00EFEFEF);
        }
    }
}

void Desktop::updateCursor(FramebufferDriver* fb, int mx, int my)
{
    if (!fb || !fb->isReady()) return;
    if (mx == m_cur_x && my == m_cur_y && m_cursor_drawn) return;

    if (m_cursor_drawn) restoreCursorBg(fb, m_cur_x, m_cur_y);

    m_cur_x = mx;
    m_cur_y = my;

    saveCursorBg(fb, mx, my);
    drawCursorAt(fb, mx, my);
    m_cursor_drawn = true;
}
