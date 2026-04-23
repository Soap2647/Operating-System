// terminal_app.cpp — Terminal Uygulaması implementasyonu
// OOP: Inheritance + Composition + Polymorphism
// Screen buffer pattern: her onDraw() buffer'dan yeniden çizer
// → Window::draw() içerik alanı silse bile terminal içeriği kaybolmaz
#include "terminal_app.hpp"
#include "theme.hpp"
#include "../../libc/include/kstring.hpp"

// ============================================================
// Constructor — RAII: buffer sıfırla
// ============================================================
TerminalApp::TerminalApp(Window* w, Shell* shell)
    : App(w), m_shell(shell), m_tb_row(0), m_tb_col(0), m_linelen(0)
    , m_hist_count(0), m_hist_idx(0)
{
    // Screen buffer'ı temizle (Encapsulation)
    for (int r = 0; r < TB_ROWS; r++) {
        for (int c = 0; c < TB_COLS; c++) {
            m_text[r][c]  = ' ';
            m_fgcol[r][c] = Theme::Text;
        }
    }
    for (int h = 0; h < HIST_SIZE; h++)
        kmemset(m_history[h], 0, 256);
    kmemset(m_linebuf, 0, sizeof(m_linebuf));

    // Hoş geldin mesajını buffer'a yaz
    write("MertOS v0.3.0 -- x86_64 Desktop\n", Theme::Cyan);
    write("Kastamonu Universitesi -- 2024\n",   Theme::Subtext);
    write("'help' yazarak komutlara bakabilirsiniz\n\n", Theme::Dim);
    showPrompt();
}

// ============================================================
// scrollBuffer — Tüm satırları bir yukarı kaydır
// ============================================================
void TerminalApp::scrollBuffer()
{
    for (int r = 0; r < TB_ROWS - 1; r++) {
        for (int c = 0; c < TB_COLS; c++) {
            m_text[r][c]  = m_text[r+1][c];
            m_fgcol[r][c] = m_fgcol[r+1][c];
        }
    }
    for (int c = 0; c < TB_COLS; c++) {
        m_text[TB_ROWS-1][c]  = ' ';
        m_fgcol[TB_ROWS-1][c] = Theme::Text;
    }
    if (m_tb_row > 0) m_tb_row--;
}

// ============================================================
// writeChar — Buffer'a karakter yaz ve imleci ilerlet
// ============================================================
void TerminalApp::writeChar(char c, uint32_t color)
{
    if (c == '\n') {
        m_tb_col = 0;
        m_tb_row++;
        if (m_tb_row >= TB_ROWS) scrollBuffer();
        return;
    }
    if (c == '\r') { m_tb_col = 0; return; }
    if (c == '\b') {
        if (m_tb_col > 0) {
            m_tb_col--;
            m_text[m_tb_row][m_tb_col]  = ' ';
            m_fgcol[m_tb_row][m_tb_col] = Theme::Text;
        }
        return;
    }

    if (m_tb_col >= TB_COLS) {
        m_tb_col = 0;
        m_tb_row++;
        if (m_tb_row >= TB_ROWS) scrollBuffer();
    }
    m_text[m_tb_row][m_tb_col]  = c;
    m_fgcol[m_tb_row][m_tb_col] = color;
    m_tb_col++;
}

// ============================================================
// write — String'i buffer'a yaz
// ============================================================
void TerminalApp::write(const char* s, uint32_t color)
{
    while (s && *s) writeChar(*s++, color);
}

// ============================================================
// showPrompt — Renkli komut istemini buffer'a yaz
// ============================================================
void TerminalApp::showPrompt()
{
    write("mert",   Theme::Blue);
    write("@",      Theme::Subtext);
    write("MertOS", Theme::Green);
    write(":$ ",    Theme::Text);
}

// ============================================================
// onDraw — Buffer'dan yeniden çiz (her çağrıda)
// Polymorphic: App::onDraw() override
// ============================================================
void TerminalApp::onDraw(FramebufferDriver* fb)
{
    if (!fb || !m_window || !m_window->visible()) return;

    int cx = m_window->contentX();
    int cy = m_window->contentY();
    int cw = m_window->contentW();
    int ch = m_window->contentH();

    fb->fillRect(cx, cy, cw, ch, Theme::Base);

    int maxRows = ch / CHAR_H;
    int maxCols = cw / CHAR_W;
    if (maxRows > TB_ROWS) maxRows = TB_ROWS;
    if (maxCols > TB_COLS) maxCols = TB_COLS;

    int startRow = 0;
    if (m_tb_row + 1 > maxRows) startRow = m_tb_row + 1 - maxRows;

    for (int r = 0; r < maxRows; r++) {
        int bufRow = startRow + r;
        if (bufRow >= TB_ROWS) break;
        for (int c = 0; c < maxCols; c++) {
            char ch_val = m_text[bufRow][c];
            if (ch_val == ' ' || ch_val == '\0') continue;
            fb->drawChar(cx + c * CHAR_W, cy + r * CHAR_H,
                         ch_val, m_fgcol[bufRow][c], Theme::Base);
        }
    }

    // İmleç (mevcut konumda alt çizgi)
    int curDispRow = m_tb_row - startRow;
    if (curDispRow >= 0 && curDispRow < maxRows && m_tb_col < maxCols) {
        fb->fillRect(cx + m_tb_col * CHAR_W,
                     cy + curDispRow * CHAR_H + CHAR_H - 2,
                     CHAR_W, 2, Theme::Blue);
    }
}

// ============================================================
// onKey — Tuş eventi işle (komut geçmişi dahil)
// Polymorphic: App::onKey() override
// ============================================================
void TerminalApp::onKey(KeyEvent& ke)
{
    if (!ke.pressed) return;

    if (ke.code == KeyCode::ENTER) {
        writeChar('\n', Theme::Text);
        m_linebuf[m_linelen] = '\0';

        // Boş olmayan komutu geçmişe kaydet
        if (m_linelen > 0) {
            if (m_hist_count < HIST_SIZE) {
                kstrncpy(m_history[m_hist_count], m_linebuf, 255);
                m_hist_count++;
            } else {
                // Geçmişi kaydır
                for (int h = 0; h < HIST_SIZE - 1; h++)
                    kstrncpy(m_history[h], m_history[h+1], 255);
                kstrncpy(m_history[HIST_SIZE-1], m_linebuf, 255);
            }
            m_hist_idx = m_hist_count;
        }

        if (m_shell && m_linelen > 0)
            m_shell->executeCommand(m_linebuf);

        m_linelen = 0;
        kmemset(m_linebuf, 0, sizeof(m_linebuf));
        writeChar('\n', Theme::Text);
        showPrompt();
    }
    else if (ke.code == KeyCode::BACKSPACE) {
        if (m_linelen > 0) {
            m_linelen--;
            m_linebuf[m_linelen] = '\0';
            writeChar('\b', Theme::Text);
        }
    }
    else if (ke.code == KeyCode::UP) {
        // Geçmişte önceki komut
        if (m_hist_count > 0 && m_hist_idx > 0) {
            m_hist_idx--;
            // Mevcut satırı sil
            while (m_linelen > 0) {
                m_linelen--;
                writeChar('\b', Theme::Text);
            }
            // Geçmiş komutu yükle
            kstrncpy(m_linebuf, m_history[m_hist_idx], 255);
            m_linelen = kstrlen(m_linebuf);
            write(m_linebuf, Theme::Text);
        }
    }
    else if (ke.code == KeyCode::DOWN) {
        // Geçmişte sonraki komut
        while (m_linelen > 0) {
            m_linelen--;
            writeChar('\b', Theme::Text);
        }
        if (m_hist_idx < m_hist_count - 1) {
            m_hist_idx++;
            kstrncpy(m_linebuf, m_history[m_hist_idx], 255);
            m_linelen = kstrlen(m_linebuf);
            write(m_linebuf, Theme::Text);
        } else {
            m_hist_idx = m_hist_count;
            kmemset(m_linebuf, 0, sizeof(m_linebuf));
            m_linelen = 0;
        }
    }
    else if (ke.ascii >= 32 && ke.ascii <= 126) {
        if (m_linelen < 255) {
            m_linebuf[m_linelen++] = ke.ascii;
            m_linebuf[m_linelen]   = '\0';
            writeChar(ke.ascii, Theme::Text);
        }
    }

    if (g_fb && m_window && m_window->visible()) {
        m_window->draw(g_fb);
        onDraw(g_fb);
    }
}

// ============================================================
// onMouse — Fare eventi (terminal için sadece focus)
// ============================================================
void TerminalApp::onMouse(MouseEvent& /*me*/)
{
}
