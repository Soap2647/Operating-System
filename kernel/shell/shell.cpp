// shell.cpp — MertSH implementasyonu

#include "shell.hpp"
#include "../memory/pmm.hpp"
#include "../memory/heap.hpp"
#include "../process/process.hpp"
#include "../desktop/app.hpp"
#include "../desktop/terminal_app.hpp"

Shell* g_shell = nullptr;

// ============================================================
// Command Table — static, tüm instance'lar paylaşır
// Command pattern: her komut kendi handler'ına pointer taşır
// ============================================================
const Shell::Command Shell::s_commands[] = {
    { "help",    "help [komut]",         "Komut listesini göster",          &Shell::cmd_help    },
    { "clear",   "clear",                "Ekranı temizle",                  &Shell::cmd_clear   },
    { "echo",    "echo [metin...]",      "Metni ekrana yaz",                &Shell::cmd_echo    },
    { "mem",     "mem",                  "Bellek istatistiklerini göster",  &Shell::cmd_mem     },
    { "ps",      "ps",                   "Process listesi",                 &Shell::cmd_ps      },
    { "ver",     "ver",                  "MertOS versiyonu",                &Shell::cmd_ver     },
    { "color",   "color [renk]",         "Prompt rengini değiştir",         &Shell::cmd_color   },
    { "alias",   "alias [k=v]",          "Alias tanımla veya listele",      &Shell::cmd_alias   },
    { "hexdump", "hexdump [addr] [n]",   "Bellek içeriğini hex yaz",        &Shell::cmd_hexdump },
    { "panic",   "panic",                "Kernel panic testi",              &Shell::cmd_panic   },
    { "reboot",  "reboot",               "Sistemi yeniden başlat",          &Shell::cmd_reboot  },
    { "halt",    "halt",                 "Sistemi durdur",                  &Shell::cmd_halt    },
};
const int Shell::s_command_count = sizeof(s_commands) / sizeof(s_commands[0]);

// ============================================================
// Yapıcı — RAII + Composition
// ============================================================
Shell::Shell(VGADriver* vga, KeyboardDriver* kbd)
    : m_vga(vga), m_kbd(kbd),
      m_argc(0), m_history_count(0), m_history_idx(-1),
      m_alias_count(0), m_prompt_color(VGAColor::Green),
      m_prompt_rgb(Color::Green), m_running(true)
{
    kmemset(m_buf, 0, INPUT_BUF_SIZE);
    for (int i = 0; i < MAX_HISTORY;  i++) m_history[i][0] = '\0';
    for (int i = 0; i < MAX_ALIASES;  i++) { m_aliases[i].from[0] = '\0'; }
}

// ============================================================
// Output helpers — FB varsa FB, yoksa VGA
// ============================================================
void Shell::out(const char* s, uint32_t fg)
{
    // Terminal buffer routing — Polymorphism: g_terminal_app->write()
    if (g_terminal_app) {
        g_terminal_app->write(s, fg);
    } else if (g_fb && g_fb->isReady()) {
        g_fb->consolePrint(s, fg);
    } else {
        m_vga->print(s);
    }
}
void Shell::outChar(char c, uint32_t fg)
{
    if (g_terminal_app) {
        g_terminal_app->writeChar(c, fg);
    } else if (g_fb && g_fb->isReady()) {
        g_fb->consolePutChar(c, fg);
    } else {
        m_vga->putChar(c);
    }
}
void Shell::outDec(uint64_t n, uint32_t fg)
{
    char buf[24]; kitoa(static_cast<long long>(n), buf, 10); out(buf, fg);
}
void Shell::outHex(uint64_t n, uint32_t fg)
{
    char buf[20]; khtoa(n, buf, false); out(buf, fg);
}
void Shell::outClear()
{
    if (g_fb && g_fb->isReady()) g_fb->consoleClear();
    else m_vga->clear();
}

// ============================================================
// printPrompt
// ============================================================
void Shell::printPrompt()
{
    out("MertSH", m_prompt_rgb);
    out("> ", Color::White);
}

// ============================================================
// echoChar — Ham karakter ekrana yaz
// ============================================================
void Shell::echoChar(char c)
{
    outChar(c);
}

// ============================================================
// readLine — Satır okuma (düzenleme desteği)
// Desteklenen özel tuşlar:
//   Backspace → son karakteri sil
//   Enter     → satırı teslim et
//   Escape    → satırı temizle
//   ↑ / ↓    → history navigasyonu
// ============================================================
int Shell::readLine()
{
    int pos = 0;
    kmemset(m_buf, 0, INPUT_BUF_SIZE);
    m_history_idx = -1; // History navigasyonunu sıfırla

    while (true) {
        KeyEvent ev;

        // Interrupt destekli bekleme
        while (!m_kbd->hasEvent())
            asm volatile("hlt");

        if (!m_kbd->getEvent(ev)) continue;
        if (!ev.pressed)          continue;

        char c = ev.ascii;

        if (c == '\n' || c == '\r') {
            // Enter → satırı teslim et
            m_buf[pos] = '\0';
            outChar('\n');
            return pos;
        }

        if (c == '\b') {
            // Backspace
            if (pos > 0) {
                pos--;
                m_buf[pos] = '\0';
                outChar('\b');
            }
            continue;
        }

        if (c == 0x1B) {
            // Escape → satırı temizle
            while (pos > 0) {
                outChar('\b');
                pos--;
            }
            kmemset(m_buf, 0, INPUT_BUF_SIZE);
            continue;
        }

        // Ok tuşları (KeyCode enum'dan)
        if (ev.code == KeyCode::UP) {
            historyGet(+1);
            while (pos > 0) { outChar('\b'); pos--; }
            kstrncpy(m_buf, m_history[m_history_idx], INPUT_BUF_SIZE);
            pos = kstrlen(m_buf);
            out(m_buf);
            continue;
        }

        if (ev.code == KeyCode::DOWN) {
            historyGet(-1);
            while (pos > 0) { outChar('\b'); pos--; }
            if (m_history_idx >= 0) {
                kstrncpy(m_buf, m_history[m_history_idx], INPUT_BUF_SIZE);
                pos = kstrlen(m_buf);
                out(m_buf);
            } else {
                m_buf[0] = '\0';
            }
            continue;
        }

        // Normal karakter
        if (c >= 32 && c < 127 && pos < INPUT_BUF_SIZE - 1) {
            m_buf[pos++] = c;
            m_buf[pos]   = '\0';
            echoChar(c);
        }
    }
}

// ============================================================
// historyPush / historyGet
// ============================================================
void Shell::historyPush(const char* line)
{
    if (!line || line[0] == '\0') return;
    // Öncekiyle aynıysa ekleme
    if (m_history_count > 0 &&
        kstrcmp(m_history[(m_history_count - 1) % MAX_HISTORY], line) == 0)
        return;

    kstrncpy(m_history[m_history_count % MAX_HISTORY], line, INPUT_BUF_SIZE);
    m_history_count++;
}

void Shell::historyGet(int delta)
{
    if (m_history_count == 0) return;

    if (m_history_idx == -1 && delta > 0)
        m_history_idx = (m_history_count - 1) % MAX_HISTORY;
    else if (delta > 0)
        m_history_idx = (m_history_idx + MAX_HISTORY - 1) % MAX_HISTORY;
    else if (delta < 0) {
        m_history_idx = (m_history_idx + 1) % MAX_HISTORY;
        if (m_history_idx == m_history_count % MAX_HISTORY)
            m_history_idx = -1;
    }
}

// ============================================================
// parseLine — Satırı argv[]'ye ayır
// Boşlukla ayrılmış token'lar, tek/çift tırnak desteği
// ============================================================
int Shell::parseLine(char* line)
{
    m_argc = 0;
    char* p = line;

    while (*p) {
        // Önce boşlukları atla
        while (*p && kisspace(*p)) { *p = '\0'; p++; }
        if (!*p) break;
        if (m_argc >= MAX_ARGS - 1) break;

        if (*p == '"' || *p == '\'') {
            // Tırnaklı string
            char quote = *p++;
            m_argv[m_argc++] = p;
            while (*p && *p != quote) p++;
            if (*p) *p++ = '\0';
        } else {
            m_argv[m_argc++] = p;
            while (*p && !kisspace(*p)) p++;
        }
    }
    m_argv[m_argc] = nullptr;
    return m_argc;
}

// ============================================================
// resolveAlias — Komut alias ise genişlet
// ============================================================
bool Shell::resolveAlias(char* line)
{
    // İlk token'ı al
    const char* cmd = kskipws(line);
    int cmd_len = 0;
    while (cmd[cmd_len] && !kisspace(cmd[cmd_len])) cmd_len++;

    for (int i = 0; i < m_alias_count; i++) {
        if (kstrncmp(m_aliases[i].from, cmd, cmd_len) == 0 &&
            m_aliases[i].from[cmd_len] == '\0') {
            // Alias bulundu — genişlet
            char rest[INPUT_BUF_SIZE];
            kstrncpy(rest, cmd + cmd_len, INPUT_BUF_SIZE);
            kstrncpy(line, m_aliases[i].to, INPUT_BUF_SIZE);
            // Rest varsa ekle
            int tolen = kstrlen(line);
            if (rest[0] && tolen < INPUT_BUF_SIZE - 2) {
                line[tolen] = ' ';
                kstrncpy(line + tolen + 1, rest, INPUT_BUF_SIZE - tolen - 1);
            }
            return true;
        }
    }
    return false;
}

// ============================================================
// executeCommand — Satırı parse et, built-in komut bul, çalıştır
// ============================================================
void Shell::executeCommand(const char* input)
{
    // Kopyala (parse in-place değiştirir)
    char line[INPUT_BUF_SIZE];
    kstrncpy(line, input, INPUT_BUF_SIZE);
    krtrim(line);
    if (line[0] == '\0') return;

    // Alias expand
    resolveAlias(line);

    // Parse
    int argc = parseLine(line);
    if (argc == 0) return;

    // Komut tablosunda ara
    for (int i = 0; i < s_command_count; i++) {
        if (kstrcmp(s_commands[i].name, m_argv[0]) == 0) {
            (this->*s_commands[i].handler)(m_argc, m_argv); // Member func ptr çağrısı
            return;
        }
    }

    // Komut bulunamadı
    out("MertSH: command not found: '", Color::Red);
    out(m_argv[0], Color::Red);
    out("'  (type help)\n", Color::Red);
}

void Shell::execute() { executeCommand(m_buf); }

// ============================================================
// run — Ana shell döngüsü
// ============================================================
void Shell::run()
{
    out("\n");
    out("  +==============================+\n", Color::Cyan);
    out("  |   MertSH -- MertOS Shell     |\n", Color::White);
    out("  |   Type 'help' to start       |\n", Color::Gray);
    out("  +==============================+\n", Color::Cyan);
    out("\n");

    while (m_running) {
        printPrompt();
        int len = readLine();
        if (len > 0) {
            historyPush(m_buf);
            execute();
        }
    }
}

// ============================================================
// ---- BUILT-IN KOMUTLAR ----
// ============================================================

// ---- help ----
void Shell::cmd_help(int argc, char** argv)
{
    if (argc >= 2) {
        for (int i = 0; i < s_command_count; i++) {
            if (kstrcmp(s_commands[i].name, argv[1]) == 0) {
                out("Usage   : ", Color::Yellow); out(s_commands[i].usage); out("\n");
                out("Desc    : ", Color::Yellow); out(s_commands[i].description); out("\n");
                return;
            }
        }
        out("Not found: ", Color::Red); out(argv[1]); out("\n");
        return;
    }

    out("MertSH Commands:\n", Color::Cyan);
    out("-----------------------------------------\n", Color::DarkGray);
    for (int i = 0; i < s_command_count; i++) {
        out("  ", Color::Gray);
        out(s_commands[i].name, Color::Yellow);
        int pad = 10 - kstrlen(s_commands[i].name);
        for (int p = 0; p < pad; p++) outChar(' ');
        out(s_commands[i].description); out("\n");
    }
    out("-----------------------------------------\n", Color::DarkGray);
    out("  Detail: help <cmd>\n");
}

// ---- clear ----
void Shell::cmd_clear(int /*argc*/, char** /*argv*/)
{
    outClear();
}

// ---- echo ----
void Shell::cmd_echo(int argc, char** argv)
{
    for (int i = 1; i < argc; i++) {
        if (i > 1) outChar(' ');
        out(argv[i]);
    }
    outChar('\n');
}

// ---- mem ----
void Shell::cmd_mem(int /*argc*/, char** /*argv*/)
{
    out("=== Memory Status ===\n", Color::Cyan);
    if (g_pmm) {
        out("PMM (Physical):\n", Color::Yellow);
        out("  Total : "); outDec(g_pmm->totalBytes() / (1024*1024)); out(" MB\n");
        out("  Used  : "); outDec(g_pmm->usedPages() * 4); out(" KB\n");
        out("  Free  : "); outDec(g_pmm->freeBytes() / (1024*1024)); out(" MB\n");
    }
    if (g_heap) {
        out("Heap (Virtual):\n", Color::Yellow);
        out("  Total : "); outDec(g_heap->totalBytes() / 1024); out(" KB\n");
        out("  Used  : "); outDec(g_heap->usedBytes()); out(" B\n");
        out("  Free  : "); outDec(g_heap->freeBytes() / 1024); out(" KB\n");
        out("  Allocs: "); outDec(g_heap->allocCount()); out("\n");
    }
}

// ---- ps ----
void Shell::cmd_ps(int /*argc*/, char** /*argv*/)
{
    if (!g_processManager) {
        out("ProcessManager not initialized!\n", Color::Red);
        return;
    }
    out("PID  STATE    NAME\n", Color::Cyan);
    out("---------------------\n");
    g_processManager->dumpProcesses();
}

// ---- ver ----
void Shell::cmd_ver(int /*argc*/, char** /*argv*/)
{
    out("MertOS ", Color::Cyan);
    out("v0.3.0-alpha", Color::Yellow);
    out(" - x86_64 Kernel\n");
    out("Arch    : x86_64\n");
    out("Build   : " __DATE__ " " __TIME__ "\n");
    out("Shell   : MertSH 1.0\n");
    out("Compiler: x86_64-elf-g++ (C++17, freestanding)\n");
}

// ---- color ----
void Shell::cmd_color(int argc, char** argv)
{
    if (argc < 2) {
        out("Renkler: black darkblue darkgreen darkcyan darkred\n");
        out("         magenta brown lightgray darkgray blue\n");
        out("         green cyan red lightmagenta yellow white\n");
        return;
    }

    struct ColorEntry { const char* name; VGAColor vga; uint32_t rgb; };
    static const ColorEntry colors[] = {
        {"black",       VGAColor::Black,        Color::Black},
        {"darkblue",    VGAColor::DarkBlue,     Color::Blue},
        {"darkgreen",   VGAColor::DarkGreen,    Color::Green},
        {"darkcyan",    VGAColor::DarkCyan,     Color::Cyan},
        {"darkred",     VGAColor::DarkRed,      Color::Red},
        {"magenta",     VGAColor::Magenta,      Color::Magenta},
        {"brown",       VGAColor::Brown,        Color::Orange},
        {"lightgray",   VGAColor::LightGray,    Color::Gray},
        {"darkgray",    VGAColor::DarkGray,     Color::DarkGray},
        {"blue",        VGAColor::Blue,         0x005555FFu},
        {"green",       VGAColor::Green,        0x0055FF55u},
        {"cyan",        VGAColor::Cyan,         0x0055FFFFu},
        {"red",         VGAColor::Red,          0x00FF5555u},
        {"lightmagenta",VGAColor::LightMagenta, 0x00FF55FFu},
        {"yellow",      VGAColor::Yellow,       Color::Yellow},
        {"white",       VGAColor::White,        Color::White},
    };

    for (auto& e : colors) {
        if (kstricmp(e.name, argv[1]) == 0) {
            m_prompt_color = e.vga;
            m_prompt_rgb   = e.rgb;
            out("Prompt rengi degistirildi.\n", e.rgb);
            return;
        }
    }
    out("Bilinmeyen renk: ", Color::Red);
    out(argv[1]);
    out("\n");
}

// ---- alias ----
void Shell::cmd_alias(int argc, char** argv)
{
    if (argc < 2) {
        // Alias listesi
        if (m_alias_count == 0) {
            out("Tanimli alias yok.\n");
            return;
        }
        for (int i = 0; i < m_alias_count; i++) {
            out(m_aliases[i].from, Color::Yellow);
            out(" = ");
            out(m_aliases[i].to);
            out("\n");
        }
        return;
    }

    // "alias k=v" formati
    // argv[1] = "k=v" ya da argc=3 -> argv[1]="k", argv[2]="v"
    const char* eq = kstrchr(argv[1], '=');
    if (!eq && argc < 3) {
        out("Kullanim: alias isim=komut\n");
        return;
    }

    char name[16], value[INPUT_BUF_SIZE];
    if (eq) {
        int nlen = static_cast<int>(eq - argv[1]);
        kstrncpy(name, argv[1], nlen + 1);
        name[nlen] = '\0';
        kstrncpy(value, eq + 1, INPUT_BUF_SIZE);
    } else {
        kstrncpy(name, argv[1], 16);
        kstrncpy(value, argv[2], INPUT_BUF_SIZE);
    }

    if (m_alias_count >= MAX_ALIASES) {
        out("Alias tablosu dolu!\n", Color::Red);
        return;
    }
    kstrncpy(m_aliases[m_alias_count].from, name,  16);
    kstrncpy(m_aliases[m_alias_count].to,   value, INPUT_BUF_SIZE);
    m_alias_count++;
    out("Alias eklendi: ", Color::Green);
    out(name); out(" = "); out(value);
    out("\n");
}

// ---- hexdump ----
void Shell::cmd_hexdump(int argc, char** argv)
{
    uint64_t addr  = (argc >= 2) ? static_cast<uint64_t>(katoi(argv[1])) : 0x100000;
    int      count = (argc >= 3) ? static_cast<int>(katoi(argv[2]))      : 128;
    if (count > 512) count = 512;

    out("Hexdump: 0x", Color::Cyan);
    outHex(addr, Color::Cyan);
    out(" (");
    outDec(count);
    out(" byte)\n");

    const uint8_t* p = reinterpret_cast<const uint8_t*>(addr);
    for (int i = 0; i < count; i += 16) {
        // Adres
        out("  ", Color::DarkGray);
        outHex(addr + i, Color::DarkGray);
        out(": ");

        // Hex
        for (int j = 0; j < 16 && i + j < count; j++) {
            char h[4];
            h[0] = "0123456789ABCDEF"[(p[i+j] >> 4) & 0xF];
            h[1] = "0123456789ABCDEF"[(p[i+j]     ) & 0xF];
            h[2] = ' '; h[3] = '\0';
            out(h);
        }
        // ASCII
        out(" |");
        for (int j = 0; j < 16 && i + j < count; j++) {
            char c = (p[i+j] >= 32 && p[i+j] < 127) ? p[i+j] : '.';
            outChar(c);
        }
        out("|\n");
    }
}

// ---- panic ----
void Shell::cmd_panic(int /*argc*/, char** /*argv*/)
{
    out("Kernel panic test baslatiliyor...\n", Color::Red);
    // Kasıtlı null pointer dereference → page fault → kernel panic
    volatile int* null_ptr = nullptr;
    (void)(*null_ptr);
}

// ---- reboot ----
void Shell::cmd_reboot(int /*argc*/, char** /*argv*/)
{
    out("Yeniden baslatiliyor...\n", Color::Yellow);
    // 8042 keyboard controller reset
    asm volatile(
        "cli\n\t"
        "1: inb $0x64, %%al\n\t"
        "test $0x02, %%al\n\t"
        "jnz 1b\n\t"
        "movb $0xFE, %%al\n\t"
        "outb %%al, $0x64\n\t"
        "hlt"
        : : : "al"
    );
}

// ---- halt ----
void Shell::cmd_halt(int /*argc*/, char** /*argv*/)
{
    out("\nSistem durduruldu. Guvenle kapatabilirsiniz.\n", Color::Yellow);
    asm volatile("cli; hlt");
}
