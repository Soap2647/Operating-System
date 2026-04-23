// settings_app.cpp — Ayarlar Uygulaması implementasyonu
// OOP: Inheritance + Polymorphism
#include "settings_app.hpp"
#include "theme.hpp"
#include "../memory/pmm.hpp"
#include "../memory/heap.hpp"
#include "../../libc/include/kstring.hpp"

extern uint64_t g_timer_ticks;

static constexpr int SIDEBAR_W = 120;
static constexpr int CAT_H     = 40;

// ============================================================
// Constructor
// ============================================================
SettingsApp::SettingsApp(Window* w)
    : App(w), m_category(ABOUT), m_bgChoice(0)
{
}

// ============================================================
// onDraw — Sidebar + içerik alanı
// Polymorphic: App::onDraw() override
// ============================================================
void SettingsApp::onDraw(FramebufferDriver* fb)
{
    if (!fb || !m_window || !m_window->visible()) return;
    drawSidebar(fb);
    drawContent(fb);
}

// ============================================================
// drawSidebar — Sol panel (120px), 4 kategori
// ============================================================
void SettingsApp::drawSidebar(FramebufferDriver* fb)
{
    int cx = m_window->contentX();
    int cy = m_window->contentY();
    int ch = m_window->contentH();

    fb->fillRect(cx, cy, SIDEBAR_W, ch, Theme::Mantle);
    fb->drawVLine(cx + SIDEBAR_W - 1, cy, ch, Theme::Surface0);

    static const char* cat_names[] = {
        "Gorunum", "Terminal", "Sistem", "Hakkinda"
    };

    for (int i = 0; i < 4; i++) {
        int ry = cy + i * CAT_H;
        bool active = (m_category == static_cast<Category>(i));

        if (active) {
            fb->fillRect(cx, ry, SIDEBAR_W - 1, CAT_H, Theme::Surface0);
            fb->fillRect(cx, ry, 3, CAT_H, Theme::Blue);
        }

        fb->drawString(cx + 12, ry + (CAT_H - 8) / 2, cat_names[i],
                       active ? Theme::Text : Theme::Subtext,
                       active ? Theme::Surface0 : Theme::Mantle);
    }
}

// ============================================================
// drawContent — Sağ panel, aktif kategoriye göre
// ============================================================
void SettingsApp::drawContent(FramebufferDriver* fb)
{
    int cx = m_window->contentX() + SIDEBAR_W;
    int cy = m_window->contentY();
    int cw = m_window->contentW() - SIDEBAR_W;
    int ch = m_window->contentH();

    fb->fillRect(cx, cy, cw, ch, Theme::Base);

    switch (m_category) {
        case APPEARANCE: drawAppearance(fb, cx, cy, cw, ch); break;
        case TERMINAL:   drawTerminal  (fb, cx, cy, cw, ch); break;
        case SYSTEM:     drawSystem    (fb, cx, cy, cw, ch); break;
        case ABOUT:      drawAbout     (fb, cx, cy, cw, ch); break;
    }
}

// ============================================================
// drawAppearance — 6 renk swatch
// ============================================================
void SettingsApp::drawAppearance(FramebufferDriver* fb, int cx, int cy, int cw, int ch)
{
    (void)cw; (void)ch;
    fb->drawString(cx + 16, cy + 16, "Arkaplan Rengi:", Theme::Text, Theme::Base);

    static const uint32_t swatches[] = {
        Theme::Base, Theme::Mantle, Theme::Crust,
        Theme::Surface0, Theme::Surface1, Theme::Overlay0
    };
    static const char* swatch_names[] = {
        "Base", "Mantle", "Crust", "Surface0", "Surface1", "Overlay0"
    };

    for (int i = 0; i < 6; i++) {
        int sx = cx + 16 + (i % 3) * 120;
        int sy = cy + 48 + (i / 3) * 80;
        bool sel = (i == m_bgChoice);

        fb->fillRect(sx, sy, 100, 50, swatches[i]);
        if (sel) fb->drawRect(sx - 2, sy - 2, 104, 54, Theme::Blue);
        fb->drawString(sx, sy + 56, swatch_names[i], Theme::Subtext, Theme::Base);
    }
}

// ============================================================
// drawTerminal — Font ölçeği ve prompt ayarları
// ============================================================
void SettingsApp::drawTerminal(FramebufferDriver* fb, int cx, int cy, int cw, int ch)
{
    (void)cw; (void)ch;
    int y = cy + 20;
    fb->drawString(cx + 16, y,      "Terminal Ayarlari", Theme::Text, Theme::Base);
    fb->drawString(cx + 16, y + 32, "Font Scale: 2x",   Theme::Subtext, Theme::Base);
    fb->drawString(cx + 16, y + 56, "Prompt: Green",    Theme::Subtext, Theme::Base);
    fb->drawString(cx + 16, y + 80, "Sekme: 4 bogosluk",Theme::Subtext, Theme::Base);
}

// ============================================================
// drawSystem — RAM, Heap, Uptime bilgileri
// ============================================================
void SettingsApp::drawSystem(FramebufferDriver* fb, int cx, int cy, int cw, int ch)
{
    (void)cw; (void)ch;
    char buf[64];
    int y = cy + 16;

    fb->drawString(cx + 16, y, "Sistem Bilgileri", Theme::Text, Theme::Base);
    y += 28;

    // RAM total
    if (g_pmm) {
        fb->drawString(cx + 16, y, "RAM Toplam: ", Theme::Subtext, Theme::Base);
        kitoa(static_cast<int64_t>(g_pmm->totalBytes() / (1024*1024)), buf, 10);
        kstrcat(buf, " MB");
        fb->drawString(cx + 16 + 96, y, buf, Theme::Text, Theme::Base);
        y += 20;

        fb->drawString(cx + 16, y, "RAM Bos:    ", Theme::Subtext, Theme::Base);
        kitoa(static_cast<int64_t>(g_pmm->freeBytes() / (1024*1024)), buf, 10);
        kstrcat(buf, " MB");
        fb->drawString(cx + 16 + 96, y, buf, Theme::Green, Theme::Base);
        y += 20;
    }

    // Heap
    if (g_heap) {
        fb->drawString(cx + 16, y, "Heap Kullan:", Theme::Subtext, Theme::Base);
        kitoa(static_cast<int64_t>(g_heap->usedBytes()), buf, 10);
        kstrcat(buf, " B");
        fb->drawString(cx + 16 + 96, y, buf, Theme::Yellow, Theme::Base);
        y += 20;
    }

    // Uptime
    uint64_t secs = g_timer_ticks / 18;
    fb->drawString(cx + 16, y, "Calisma Sur:", Theme::Subtext, Theme::Base);
    kitoa(static_cast<int64_t>(secs), buf, 10);
    kstrcat(buf, " s");
    fb->drawString(cx + 16 + 96, y, buf, Theme::Cyan, Theme::Base);
    y += 20;

    fb->drawString(cx + 16, y, "Mimari: x86_64", Theme::Dim, Theme::Base);
}

// ============================================================
// drawAbout — Midterm "altın" sayfası: OOP kavramları
// ============================================================
void SettingsApp::drawAbout(FramebufferDriver* fb, int cx, int cy, int cw, int ch)
{
    (void)cw; (void)ch;
    int x = cx + 12;
    int y = cy + 8;

    // Başlık
    fb->fillRect(cx + 8, cy + 6, cw - 24, 28, Theme::Surface0);
    fb->drawString(x + 4, y + 4, "MertOS v0.3.0  --  x86_64 Desktop Kernel", Theme::Blue,    Theme::Surface0);
    fb->drawString(x + 4, y + 14, "Kastamonu Universitesi  |  2024",           Theme::Subtext, Theme::Surface0);
    y += 40;

    // OOP badge'leri — sol sütun
    struct OOPEntry {
        const char* tag;
        uint32_t    tag_color;
        const char* lines[3];
        uint32_t    line_color;
    };
    static const OOPEntry entries[] = {
        { "[INHERITANCE]",   Theme::Mauve,  {
            "TerminalApp, FileManager,",
            "SettingsApp, MscApp : App",
            "MouseDriver : Driver"
        }, Theme::Text },
        { "[POLYMORPHISM]",  Theme::Blue,   {
            "App::onDraw()  -- 4 impl.",
            "App::onKey()   -- 4 impl.",
            "Widget::draw() -- virtual"
        }, Theme::Text },
        { "[ENCAPSULATION]", Theme::Green,  {
            "MouseDriver::m_packet[]",
            "TerminalApp::m_text[][]",
            "Shell cmd table (private)"
        }, Theme::Text },
        { "[COMPOSITION]",   Theme::Yellow, {
            "AppManager HAS-A App[8]",
            "Desktop HAS-A DesktopIcon[8]",
            "TerminalApp HAS-A Shell"
        }, Theme::Text },
        { "[TEMPLATE]",      Theme::Cyan,   {
            "MessageQueue<Event,64>",
            "= EventQueue (type-safe)",
            ""
        }, Theme::Text },
        { "[RAII]",          Theme::Peach,  {
            "VFS() -> creates root /",
            "Heap() -> reserves memory",
            "PMM()  -> inits bitmap"
        }, Theme::Text },
    };

    int col_w = (cw - 24) / 2;
    for (int i = 0; i < 6; i++) {
        int col = i % 2;
        int row = i / 2;
        int bx  = cx + 8 + col * col_w;
        int by  = y + row * 56;
        const OOPEntry& e = entries[i];

        // Kutu arka planı
        fb->fillRect(bx, by, col_w - 4, 52, Theme::Mantle);
        // Sol renk çizgisi
        fb->fillRect(bx, by, 3, 52, e.tag_color);
        // Tag
        fb->drawString(bx + 6, by + 4, e.tag, e.tag_color, Theme::Mantle);
        // Satırlar
        for (int li = 0; li < 3; li++) {
            if (e.lines[li][0] == '\0') continue;
            fb->drawString(bx + 6, by + 16 + li * 12, e.lines[li],
                           e.line_color, Theme::Mantle);
        }
    }
}

// ============================================================
// onKey — Numara tuşları 1-4 kategori değiştirir
// ============================================================
void SettingsApp::onKey(KeyEvent& ke)
{
    if (!ke.pressed) return;

    if (ke.ascii == '1') { m_category = APPEARANCE; if (g_fb && m_window->visible()) onDraw(g_fb); }
    else if (ke.ascii == '2') { m_category = TERMINAL;   if (g_fb && m_window->visible()) onDraw(g_fb); }
    else if (ke.ascii == '3') { m_category = SYSTEM;     if (g_fb && m_window->visible()) onDraw(g_fb); }
    else if (ke.ascii == '4') { m_category = ABOUT;      if (g_fb && m_window->visible()) onDraw(g_fb); }
}

// ============================================================
// onMouse — Sidebar tıklaması kategori değiştirir
// ============================================================
void SettingsApp::onMouse(MouseEvent& me)
{
    if (!me.clicked || !m_window) return;

    int cx = m_window->contentX();
    int cy = m_window->contentY();

    if (me.x >= cx && me.x < cx + SIDEBAR_W) {
        int rel_y = me.y - cy;
        if (rel_y >= 0) {
            int idx = rel_y / CAT_H;
            if (idx >= 0 && idx <= 3) {
                m_category = static_cast<Category>(idx);
                if (g_fb && m_window->visible()) onDraw(g_fb);
            }
        }
    }
}
