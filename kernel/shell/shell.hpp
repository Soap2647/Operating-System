// shell.hpp — MertSH Command Shell
//
// OOP Bağlantısı (midterm):
//   Encapsulation : input buffer, history, command table → private
//   Composition   : Shell HAS-A VGADriver*, HAS-A KeyboardDriver*
//   Command pattern: Command struct (isim + handler function pointer)

#pragma once
#include "../drivers/vga.hpp"
#include "../drivers/keyboard.hpp"
#include "../drivers/framebuffer.hpp"
#include "../../libc/include/kstring.hpp"

// ============================================================
// Shell Sınıfı — MertSH
// ============================================================
class Shell {
public:
    static constexpr int INPUT_BUF_SIZE = 256;
    static constexpr int MAX_ARGS       = 16;
    static constexpr int MAX_HISTORY    = 8;
    static constexpr int MAX_ALIASES    = 8;

    // ---- Yapıcı (RAII + Composition) ----
    // vga ve kbd: Shell'in bağımlılıkları — constructor'a inject edilir
    Shell(VGADriver* vga, KeyboardDriver* kbd);

    // ---- Ana döngü — hiç dönmez ----
    void run();

    // ---- Tek komut çalıştır (process'ten çağrı için) ----
    void executeCommand(const char* line);

private:
    // ============================================================
    // Command Pattern — OOP tasarım deseni
    // Her built-in komut bu yapıyla temsil edilir
    // ============================================================
    struct Command {
        const char* name;                               // "help", "clear" vb.
        const char* usage;                              // "echo [metin]"
        const char* description;                        // Kısa açıklama
        void (Shell::*handler)(int argc, char** argv);  // Member func ptr
    };

    // Alias (kısayol komutlar)
    struct Alias {
        char from[16];
        char to[INPUT_BUF_SIZE];
    };

    // ---- State (Encapsulation: tamamen private) ----
    VGADriver*        m_vga;
    KeyboardDriver*   m_kbd;

    // ---- Output helpers — FB varsa FB, yoksa VGA ----
    void out(const char* s, uint32_t fg = Color::White);
    void outChar(char c,    uint32_t fg = Color::White);
    void outDec(uint64_t n, uint32_t fg = Color::White);
    void outHex(uint64_t n, uint32_t fg = Color::White);
    void outClear();

    char  m_buf[INPUT_BUF_SIZE];    // Aktif satır tamponu
    char* m_argv[MAX_ARGS];         // Parse edilmiş argümanlar
    int   m_argc;

    // Command history (basit ring buffer)
    char m_history[MAX_HISTORY][INPUT_BUF_SIZE];
    int  m_history_count;
    int  m_history_idx;             // Navigasyon pozisyonu

    // Alias tablosu
    Alias m_aliases[MAX_ALIASES];
    int   m_alias_count;

    // Prompt rengi (color komutu değiştirebilir)
    VGAColor m_prompt_color;  // VGA için
    uint32_t m_prompt_rgb;    // Framebuffer için
    bool     m_running;

    // ---- Command Table (static — tüm instance'lar paylaşır) ----
    static const Command s_commands[];
    static const int     s_command_count;

    // ---- I/O ----
    void printPrompt();
    int  readLine();                // '\n' gelene kadar oku, düzenleme destekli
    void echoChar(char c);

    // ---- Parser ----
    int  parseLine(char* line);     // Satırı argv[]'ye böl, argc döndür

    // ---- Executor ----
    void execute();
    bool resolveAlias(char* line);  // Alias varsa expand et

    // History yönetimi
    void historyPush(const char* line);
    void historyGet(int delta);     // +1 ileri, -1 geri

    // ---- Built-in Komutlar ----
    void cmd_help    (int argc, char** argv);  // Komut listesi
    void cmd_clear   (int argc, char** argv);  // Ekranı temizle
    void cmd_echo    (int argc, char** argv);  // Argümanları yaz
    void cmd_mem     (int argc, char** argv);  // PMM + Heap istatistik
    void cmd_ps      (int argc, char** argv);  // Process listesi
    void cmd_ver     (int argc, char** argv);  // MertOS versiyonu
    void cmd_reboot  (int argc, char** argv);  // Yeniden başlat
    void cmd_halt    (int argc, char** argv);  // Sistemi durdur
    void cmd_color   (int argc, char** argv);  // Prompt rengini değiştir
    void cmd_alias   (int argc, char** argv);  // Alias tanımla / listele
    void cmd_panic   (int argc, char** argv);  // Kernel panic test
    void cmd_hexdump (int argc, char** argv);  // Bellek hex dump
};

// Global shell instance
extern Shell* g_shell;
