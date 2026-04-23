// desktop.hpp — Desktop ortamı (yeniden tasarlandı)
#pragma once
#include "widget.hpp"
#include "window.hpp"
#include "theme.hpp"
#include "app.hpp"

extern uint64_t g_timer_ticks;

static constexpr int MAX_WINDOWS = 8;

// Desktop ikonu
struct DesktopIcon {
    char     label[32];
    uint32_t color;
    char     symbol;
    int      app_idx;  // AppManager'daki index (-1 = henüz açılmadı)
    int      x, y;
};

// Pencere önceki konumu (maximize/restore için)
struct WinState {
    int  x, y, w, h;
    bool saved;
};

class Desktop : public Widget {
public:
    static constexpr int TOPBAR_H = 32;  // Üst bilgi çubuğu
    static constexpr int DOCK_H   = 56;  // Alt dock

    Desktop();
    void draw(FramebufferDriver* fb) override;
    void handleClick(int mx, int my);
    void handleMouseMove(int mx, int my);
    void handleMouseRelease(int mx, int my);

    // Cursor güncelle
    void updateCursor(FramebufferDriver* fb, int mx, int my);

    // Cursor başlangıç pozisyonunu ayarla (mouse driver ile senkron)
    void setCursorPos(int x, int y) {
        m_cur_x = x; m_cur_y = y; m_cursor_drawn = false;
    }

    // Icon ekle (launch callback index)
    void addIcon(const char* label, uint32_t color, char symbol, int app_idx);

private:
    void drawTopBar(FramebufferDriver* fb);
    void drawDesktopArea(FramebufferDriver* fb);
    void drawDock(FramebufferDriver* fb);
    void drawCursorAt(FramebufferDriver* fb, int x, int y);
    void saveCursorBg(FramebufferDriver* fb, int x, int y);
    void restoreCursorBg(FramebufferDriver* fb, int x, int y);
    void drawBackground(FramebufferDriver* fb);

    // Icons
    DesktopIcon m_icons[8];
    int         m_icon_count;
    int         m_icon_hover;   // Hangi masaüstü ikonunun üzerinde (-1 = yok)

    // Drag state (pencere sürükleme)
    int  m_drag_app;          // -1 = sürükleme yok
    int  m_drag_start_x;
    int  m_drag_start_y;
    int  m_drag_win_start_x;
    int  m_drag_win_start_y;

    // Çift tıklama maximize/restore
    uint64_t m_last_click_time;
    int      m_last_click_app;
    WinState m_saved_state[8]; // Her app için pencere restore state'i

    // Dock hover
    int m_dock_hover;

    // Cursor
    static constexpr int CUR_W = 12;
    static constexpr int CUR_H = 20;
    uint32_t m_cursor_bg[CUR_W * CUR_H];
    int  m_cur_x, m_cur_y;
    bool m_cursor_drawn;
};

extern Desktop* g_desktop;
