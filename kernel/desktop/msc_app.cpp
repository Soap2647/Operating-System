// msc_app.cpp — MertStudio Code Editör implementasyonu
// OOP: Inheritance + Polymorphism
#include "msc_app.hpp"
#include "theme.hpp"
#include "../../libc/include/kstring.hpp"

static constexpr int LINE_H    = 16;  // Satır yüksekliği (piksel)
static constexpr int CHAR_W    = 8;   // Karakter genişliği

// ============================================================
// Constructor
// ============================================================
MertStudioCode::MertStudioCode(Window* w)
    : App(w), m_textlen(0), m_cursor(0)
{
    kmemset(m_text, 0, sizeof(m_text));
    // Başlangıç içeriği
    const char* welcome = "// MertStudio Code\n// MertOS v0.3.0\n\n";
    int wlen = kstrlen(welcome);
    if (wlen < TEXT_SIZE - 1) {
        kstrncpy(m_text, welcome, TEXT_SIZE - 1);
        m_textlen = wlen;
        m_cursor  = wlen;
    }
}

// ============================================================
// lineCount — Metin içindeki satır sayısını hesapla
// ============================================================
int MertStudioCode::lineCount() const
{
    int lines = 1;
    for (int i = 0; i < m_textlen; i++) {
        if (m_text[i] == '\n') lines++;
    }
    return lines;
}

// ============================================================
// posToLineCol — Metin pozisyonunu satır/sütuna çevir
// ============================================================
void MertStudioCode::posToLineCol(int pos, int& line, int& col) const
{
    line = 0;
    col  = 0;
    for (int i = 0; i < pos && i < m_textlen; i++) {
        if (m_text[i] == '\n') {
            line++;
            col = 0;
        } else {
            col++;
        }
    }
}

// ============================================================
// onDraw — Editör alanını çiz: gutter + metin + imleç
// Polymorphic: App::onDraw() override
// ============================================================
void MertStudioCode::onDraw(FramebufferDriver* fb)
{
    if (!fb || !m_window || !m_window->visible()) return;

    int cx = m_window->contentX();
    int cy = m_window->contentY();
    int cw = m_window->contentW();
    int ch = m_window->contentH();

    // İçerik arka planı (Surface0)
    fb->fillRect(cx, cy, cw, ch, Theme::Surface0);

    // Gutter (sol satır numaraları, Surface1 arka plan)
    fb->fillRect(cx, cy, GUTTER_W, ch, Theme::Surface1);
    fb->drawVLine(cx + GUTTER_W, cy, ch, Theme::Surface0);

    // Metin alanı başlangıcı
    int tx = cx + GUTTER_W + 4;
    int ty = cy;

    // Metin satırlarını çiz
    int line    = 0;
    int col     = 0;
    int py      = ty;      // Mevcut y
    int line_px = tx;      // Mevcut satırın x başlangıcı

    // Satır numarası çiz (1 tabanlı)
    {
        char lnbuf[8];
        kitoa(1, lnbuf, 10);
        fb->drawString(cx + 2, py, lnbuf, Theme::Dim, Theme::Surface1);
    }

    for (int i = 0; i <= m_textlen; i++) {
        // İmleç çiz
        if (i == m_cursor) {
            int cur_x = line_px + col * CHAR_W;
            fb->fillRect(cur_x, py, 2, LINE_H, Theme::Text);
        }

        if (i == m_textlen) break;

        char c = m_text[i];
        if (c == '\n') {
            line++;
            col = 0;
            py += LINE_H;
            line_px = tx;

            if (py + LINE_H > cy + ch) break; // Görünür alan dışı

            // Yeni satır numarası
            char lnbuf[8];
            kitoa(static_cast<int64_t>(line + 1), lnbuf, 10);
            fb->drawString(cx + 2, py, lnbuf, Theme::Dim, Theme::Surface1);
        } else {
            int cx2 = line_px + col * CHAR_W;
            if (cx2 + CHAR_W <= cx + cw) {
                // Yorum satırı rengi
                bool in_comment = false;
                if (col == 0 && c == '/' && i + 1 < m_textlen && m_text[i+1] == '/') {
                    in_comment = true;
                }
                uint32_t color = in_comment ? Theme::Dim : Theme::Text;
                // Basit sözdizimi renklendirme: // ile başlayan satır
                // (Tam satır kontrolü çok karmaşık, basit karakter çizimi yeterli)
                fb->drawChar(cx2, py, c, color, Theme::Surface0);
            }
            col++;
        }
    }
}

// ============================================================
// onKey — Karakter ekle/sil/yeni satır
// ============================================================
void MertStudioCode::onKey(KeyEvent& ke)
{
    if (!ke.pressed) return;

    if (ke.code == KeyCode::ENTER) {
        // Yeni satır ekle
        if (m_textlen < TEXT_SIZE - 1) {
            // İmleç konumuna '\n' ekle
            for (int i = m_textlen; i > m_cursor; i--)
                m_text[i] = m_text[i-1];
            m_text[m_cursor] = '\n';
            m_textlen++;
            m_cursor++;
            m_text[m_textlen] = '\0';
        }
    }
    else if (ke.code == KeyCode::BACKSPACE) {
        if (m_cursor > 0) {
            for (int i = m_cursor - 1; i < m_textlen - 1; i++)
                m_text[i] = m_text[i+1];
            m_textlen--;
            m_cursor--;
            m_text[m_textlen] = '\0';
        }
    }
    else if (ke.code == KeyCode::LEFT) {
        if (m_cursor > 0) m_cursor--;
    }
    else if (ke.code == KeyCode::RIGHT) {
        if (m_cursor < m_textlen) m_cursor++;
    }
    else if (ke.ascii >= 32 && ke.ascii <= 126) {
        // Yazdırılabilir karakter ekle
        if (m_textlen < TEXT_SIZE - 1) {
            for (int i = m_textlen; i > m_cursor; i--)
                m_text[i] = m_text[i-1];
            m_text[m_cursor] = ke.ascii;
            m_textlen++;
            m_cursor++;
            m_text[m_textlen] = '\0';
        }
    }

    // Yeniden çiz
    if (g_fb && m_window && m_window->visible()) {
        m_window->draw(g_fb);
        onDraw(g_fb);
    }
}

// ============================================================
// onMouse — Fare olayları (şimdilik yok)
// ============================================================
void MertStudioCode::onMouse(MouseEvent& /*me*/)
{
    // MSC editörü fare olaylarını şimdilik işlemiyor
}
