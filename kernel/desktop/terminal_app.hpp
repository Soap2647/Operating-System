// terminal_app.hpp — Terminal Uygulaması
// OOP: TerminalApp : public App (INHERITANCE)
//      Composition: TerminalApp HAS-A Shell, HAS-A screen buffer
//      Encapsulation: m_text/m_fgcol buffer private
#pragma once
#include "app.hpp"
#include "../shell/shell.hpp"

// ============================================================
// TerminalApp — Terminal emulator uygulaması
// Screen buffer: 30x80 karakter, her draw'da yeniden çizilir
// OOP: Inheritance (App), Composition (Shell + buffer)
// ============================================================
class TerminalApp : public App {
public:
    static constexpr int TB_ROWS = 30;   // Buffer satır sayısı
    static constexpr int TB_COLS = 80;   // Buffer sütun sayısı
    static constexpr int CHAR_W  = 8;    // Piksel/karakter genişliği (scale=1)
    static constexpr int CHAR_H  = 8;    // Piksel/karakter yüksekliği (scale=1)

    // Composition: constructor'a Shell inject edilir
    TerminalApp(Window* w, Shell* shell);

    // ---- App interface override'ları (POLYMORPHISM) ----
    void        onDraw(FramebufferDriver* fb) override;
    void        onKey(KeyEvent& ke)           override;
    void        onMouse(MouseEvent& me)       override;
    const char* getName()   const override { return "Terminal"; }
    uint32_t    getColor()  const override { return 0x00CBA6F7; }  // Mauve
    char        getSymbol() const override { return 'T'; }

    // Shell output routing — Shell::out() buraya yazar
    void write(const char* s, uint32_t color);
    void writeChar(char c, uint32_t color);

    // Prompt göster
    void showPrompt();

private:
    Shell* m_shell;          // Composition: HAS-A Shell

    // Screen buffer (Encapsulation — private)
    char     m_text[TB_ROWS][TB_COLS];   // Karakter içeriği
    uint32_t m_fgcol[TB_ROWS][TB_COLS];  // Her karakterin rengi
    int      m_tb_row;   // Şu anki yazma satırı
    int      m_tb_col;   // Şu anki yazma sütunu

    // Komut satırı tamponu
    char m_linebuf[256];
    int  m_linelen;

    // Komut geçmişi (↑↓ ile erişim)
    static constexpr int HIST_SIZE = 16;
    char m_history[HIST_SIZE][256];
    int  m_hist_count;
    int  m_hist_idx;

    // Scroll: tüm satırları bir yukarı kaydır
    void scrollBuffer();

    // Görünür alan hesabı
    int visibleCols(int content_w) const { return content_w / CHAR_W; }
    int visibleRows(int content_h) const { return content_h / CHAR_H; }
};
