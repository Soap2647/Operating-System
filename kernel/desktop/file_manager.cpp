// file_manager.cpp — Dosya Yöneticisi implementasyonu
// OOP: Inheritance + Composition + Polymorphism
#include "file_manager.hpp"
#include "theme.hpp"
#include "../../libc/include/kstring.hpp"

static constexpr int TOOLBAR_H  = 28;
static constexpr int SIDEBAR_W  = 140;
static constexpr int ROW_H      = 22;

// ============================================================
// Constructor
// ============================================================
FileManager::FileManager(Window* w, VFS* vfs)
    : App(w), m_vfs(vfs), m_selectedItem(0), m_dirty(true)
{
    m_currentDir = vfs ? vfs->getRoot() : nullptr;
}

// ============================================================
// buildPath — INode'dan path string oluştur (recursive)
// ============================================================
void FileManager::buildPath(INode* node, char* buf, int bufsize)
{
    if (!node || bufsize <= 0) return;

    if (!node->parent) {
        // Kök
        kstrncpy(buf, "/", bufsize);
        return;
    }

    // Özyinelemeli olarak üst dizini ekle
    buildPath(node->parent, buf, bufsize);

    int len = kstrlen(buf);
    if (len > 0 && buf[len-1] != '/') {
        if (len < bufsize - 1) { buf[len] = '/'; buf[len+1] = '\0'; len++; }
    }

    int remaining = bufsize - len - 1;
    if (remaining > 0) {
        kstrncpy(buf + len, node->name, remaining);
    }
}

// ============================================================
// onDraw — İki panelli düzen çiz
// Polymorphic: App::onDraw() override
// ============================================================
void FileManager::onDraw(FramebufferDriver* fb)
{
    if (!fb || !m_window || !m_window->visible()) return;

    drawToolbar(fb);
    drawSidebar(fb);
    drawFileList(fb);
}

// ============================================================
// drawToolbar — Üst araç çubuğu (28px)
// ============================================================
void FileManager::drawToolbar(FramebufferDriver* fb)
{
    int cx = m_window->contentX();
    int cy = m_window->contentY();
    int cw = m_window->contentW();

    // Arka plan
    fb->fillRect(cx, cy, cw, TOOLBAR_H, Theme::Surface0);
    // Alt kenarlık
    fb->drawHLine(cx, cy + TOOLBAR_H - 1, cw, Theme::Surface1);

    // Navigasyon butonları
    int bx = cx + 8;
    int by = cy + (TOOLBAR_H - 16) / 2;
    fb->drawString(bx,      by, "<", Theme::Blue, Theme::Surface0);
    fb->drawString(bx + 16, by, ">", Theme::Blue, Theme::Surface0);
    fb->drawString(bx + 32, by, "^", Theme::Blue, Theme::Surface0);

    // Mevcut path
    char pathbuf[128];
    kmemset(pathbuf, 0, sizeof(pathbuf));
    buildPath(m_currentDir, pathbuf, 127);

    fb->drawString(cx + 80, cy + (TOOLBAR_H - 8) / 2, pathbuf, Theme::Text, Theme::Surface0);
}

// ============================================================
// drawSidebar — Sol panel (140px), hızlı erişim dizinleri
// ============================================================
void FileManager::drawSidebar(FramebufferDriver* fb)
{
    int cx = m_window->contentX();
    int cy = m_window->contentY() + TOOLBAR_H;
    int ch = m_window->contentH() - TOOLBAR_H;

    // Arka plan
    fb->fillRect(cx, cy, SIDEBAR_W, ch, Theme::Mantle);
    // Sağ kenarlık
    fb->drawVLine(cx + SIDEBAR_W - 1, cy, ch, Theme::Surface0);

    // Sabit dizinler
    static const char* dirs[] = { "/", "/documents", "/programs", "/system" };
    static const int   dir_count = 4;

    for (int i = 0; i < dir_count; i++) {
        int ry = cy + i * (ROW_H + 2);
        bool selected = false;

        // Mevcut dizinle eşleşiyor mu?
        if (m_vfs) {
            INode* node = m_vfs->open(dirs[i]);
            if (node && node == m_currentDir) selected = true;
        }

        if (selected) {
            fb->fillRect(cx, ry, SIDEBAR_W - 1, ROW_H, Theme::Surface0);
            // Sol accent çizgisi
            fb->fillRect(cx, ry, 3, ROW_H, Theme::Blue);
        }

        fb->drawString(cx + 10, ry + (ROW_H - 8) / 2, dirs[i],
                       selected ? Theme::Text : Theme::Subtext,
                       selected ? Theme::Surface0 : Theme::Mantle);
    }
}

// ============================================================
// drawFileList — Sağ panel: dosya listesi
// ============================================================
void FileManager::drawFileList(FramebufferDriver* fb)
{
    if (!m_currentDir) return;

    int cx = m_window->contentX() + SIDEBAR_W;
    int cy = m_window->contentY() + TOOLBAR_H;
    int cw = m_window->contentW() - SIDEBAR_W;
    int ch = m_window->contentH() - TOOLBAR_H;

    // Arka plan
    fb->fillRect(cx, cy, cw, ch, Theme::Base);

    if (m_currentDir->child_count == 0) {
        fb->drawString(cx + 20, cy + 20, "(bos dizin)", Theme::Dim, Theme::Base);
        return;
    }

    for (int i = 0; i < m_currentDir->child_count; i++) {
        INode* node = m_currentDir->children[i];
        if (!node) continue;

        int ry = cy + i * (ROW_H + 2);
        if (ry + ROW_H > cy + ch) break; // Taşma

        bool selected = (i == m_selectedItem);

        // Seçili satır arka planı
        if (selected) {
            fb->fillRect(cx, ry, cw, ROW_H, Theme::Surface0);
        }

        // İkon (renkli 12x12 küçük rect)
        int ix = cx + 8;
        int iy = ry + (ROW_H - 12) / 2;

        if (node->is_dir) {
            // Dizin: sarı yuvarlatılmış ikon
            fb->fillRect(ix+2,  iy,     8,  12, Theme::Yellow);
            fb->fillRect(ix,    iy+2,   12,  8, Theme::Yellow);
            fb->fillRect(ix+2,  iy+2,   8,   8, Theme::Yellow);
        } else {
            // Dosya uzantısına göre renk
            const char* name = node->name;
            int nlen = kstrlen(name);
            uint32_t file_color = Theme::Subtext;
            if (nlen > 4) {
                const char* ext = name + nlen - 4;
                if (ext[0]=='.' && ext[1]=='c' && ext[2]=='p' && ext[3]=='p')
                    file_color = Theme::Blue;
                else if (ext[0]=='.' && ext[1]=='h' && ext[2]=='p' && ext[3]=='p')
                    file_color = Theme::Cyan;
                else if (ext[0]=='.' && ext[1]=='t' && ext[2]=='x' && ext[3]=='t')
                    file_color = Theme::Green;
                else if (ext[0]=='.' && ext[1]=='d' && ext[2]=='r' && ext[3]=='k')
                    file_color = Theme::Mauve;
            }
            fb->fillRect(ix+2,  iy,     8,  12, file_color);
            fb->fillRect(ix,    iy+2,   12,  8,  file_color);
            fb->fillRect(ix+2,  iy+2,   8,   8,  file_color);
        }

        // Dosya adı
        fb->drawString(ix + 16, ry + (ROW_H - 8) / 2, node->name,
                       selected ? Theme::Text : Theme::Subtext,
                       selected ? Theme::Surface0 : Theme::Base);

        // Suffix (DIR veya boyut)
        if (node->is_dir) {
            fb->drawString(cx + cw - 50, ry + (ROW_H - 8) / 2, "DIR",
                           Theme::Yellow,
                           selected ? Theme::Surface0 : Theme::Base);
        } else if (node->size > 0) {
            char sizebuf[16];
            kitoa(static_cast<int64_t>(node->size), sizebuf, 10);
            kstrcat(sizebuf, "B");
            fb->drawString(cx + cw - 60, ry + (ROW_H - 8) / 2, sizebuf,
                           Theme::Dim,
                           selected ? Theme::Surface0 : Theme::Base);
        }
    }

    // ---- Durum çubuğu (alt, 18px) ----
    int status_y = cy + ch - 18;
    fb->fillRect(cx, status_y, cw, 18, Theme::Mantle);
    fb->drawHLine(cx, status_y, cw, Theme::Surface0);

    char statusbuf[64];
    statusbuf[0] = '\0';
    char countbuf[8];
    kitoa(m_currentDir->child_count, countbuf, 10);
    kstrcat(statusbuf, countbuf);
    kstrcat(statusbuf, " oge");
    if (m_selectedItem < m_currentDir->child_count && m_currentDir->child_count > 0) {
        kstrcat(statusbuf, "  |  Secili: ");
        kstrcat(statusbuf, m_currentDir->children[m_selectedItem]->name);
    }
    fb->drawString(cx + 8, status_y + 5, statusbuf, Theme::Subtext, Theme::Mantle);
}

// ============================================================
// onKey — Klavye eventi
// ============================================================
void FileManager::onKey(KeyEvent& ke)
{
    if (!ke.pressed || !m_currentDir) return;

    if (ke.code == KeyCode::UP) {
        if (m_selectedItem > 0) m_selectedItem--;
        m_dirty = true;
        if (g_fb && m_window->visible()) onDraw(g_fb);
    }
    else if (ke.code == KeyCode::DOWN) {
        if (m_selectedItem < m_currentDir->child_count - 1) m_selectedItem++;
        m_dirty = true;
        if (g_fb && m_window->visible()) onDraw(g_fb);
    }
    else if (ke.code == KeyCode::ENTER) {
        // Seçili öğeye gir
        if (m_selectedItem < m_currentDir->child_count) {
            INode* node = m_currentDir->children[m_selectedItem];
            if (node && node->is_dir) {
                m_currentDir  = node;
                m_selectedItem = 0;
                m_dirty = true;
                if (g_fb && m_window->visible()) onDraw(g_fb);
            }
        }
    }
    else if (ke.ascii == '\b' || ke.code == KeyCode::BACKSPACE) {
        // Üst dizine git
        if (m_currentDir && m_currentDir->parent) {
            m_currentDir  = m_currentDir->parent;
            m_selectedItem = 0;
            m_dirty = true;
            if (g_fb && m_window->visible()) onDraw(g_fb);
        }
    }
}

// ============================================================
// onMouse — Fare eventi
// ============================================================
void FileManager::onMouse(MouseEvent& me)
{
    if (!me.clicked || !m_currentDir || !m_window) return;

    int cx = m_window->contentX() + SIDEBAR_W;
    int cy = m_window->contentY() + TOOLBAR_H;
    int ch = m_window->contentH() - TOOLBAR_H;

    // Sidebar tıklaması
    static const char* dirs[] = { "/", "/documents", "/programs", "/system" };
    int sidebar_cx = m_window->contentX();
    int sidebar_cy = m_window->contentY() + TOOLBAR_H;

    for (int i = 0; i < 4; i++) {
        int ry = sidebar_cy + i * (ROW_H + 2);
        if (me.x >= sidebar_cx && me.x < sidebar_cx + SIDEBAR_W &&
            me.y >= ry && me.y < ry + ROW_H) {
            if (m_vfs) {
                INode* node = m_vfs->open(dirs[i]);
                if (node) {
                    m_currentDir  = node;
                    m_selectedItem = 0;
                    if (g_fb && m_window->visible()) onDraw(g_fb);
                }
            }
            return;
        }
    }

    // Dosya listesi tıklaması
    if (me.x >= cx && me.x < cx + m_window->contentW() - SIDEBAR_W) {
        int rel_y = me.y - cy;
        if (rel_y >= 0 && rel_y < ch) {
            int idx = rel_y / (ROW_H + 2);
            if (idx < m_currentDir->child_count) {
                m_selectedItem = idx;
                if (g_fb && m_window->visible()) onDraw(g_fb);
            }
        }
    }
}
