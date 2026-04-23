// kernel.cpp — MertOS Kernel Entry Point (v3)
// Tüm subsystem'leri init sırasıyla başlatır.
// OOP özeti:
//   Inheritance:   VGADriver, KeyboardDriver : public Driver
//   Polymorphism:  DriverManager::initializeAll() → virtual initialize()
//   Encapsulation: PMM bitmap, VMM page tables, Process contexts, Heap blocks
//   Template:      MessageQueue<T,N>
//   RAII:          PMM/GDT/Heap constructor'da hemen hazır, placement new kaldı

#include "arch/x86_64/gdt.hpp"
#include "arch/x86_64/idt.hpp"
#include "drivers/driver.hpp"
#include "drivers/vga.hpp"
#include "drivers/keyboard.hpp"
#include "drivers/framebuffer.hpp"
#include "drivers/mouse.hpp"
#include "memory/multiboot2.hpp"
#include "memory/pmm.hpp"
#include "memory/vmm.hpp"
#include "memory/heap.hpp"
#include "process/process.hpp"
#include "ipc/message_queue.hpp"
#include "shell/shell.hpp"
#include "desktop/desktop.hpp"
#include "desktop/splash.hpp"
#include "fs/vfs.hpp"
#include "desktop/event.hpp"
#include "desktop/app.hpp"
#include "desktop/terminal_app.hpp"
#include "desktop/file_manager.hpp"
#include "desktop/settings_app.hpp"
#include "desktop/msc_app.hpp"

// ============================================================
// Kernel versiyonu
// ============================================================
static constexpr const char* MERTOS_VERSION = "0.3.0";
uint64_t g_timer_ticks = 0;

// ============================================================
// Output helpers — dispatch to FB when ready, else VGA
// ============================================================
static void kprint(const char* s) {
    if (g_fb && g_fb->isReady()) g_fb->consolePrint(s, Color::White);
    else if (g_vga)              g_vga->print(s);
}
static void kprintCol(const char* s, uint32_t rgb, VGAColor vga) {
    if (g_fb && g_fb->isReady()) g_fb->consolePrint(s, rgb);
    else if (g_vga)              g_vga->printColored(s, vga);
}
static void kprintDec(uint64_t n) {
    if (n == 0) { kprint("0"); return; }
    char buf[22]; int i = 0;
    while (n > 0) { buf[i++] = '0' + (n % 10); n /= 10; }
    char rev[22]; int j = 0;
    while (i-- > 0) rev[j++] = buf[i];
    rev[j] = '\0';
    kprint(rev);
}
static void kprintHex(uint64_t n) {
    const char* hex = "0123456789ABCDEF";
    bool leading = true;
    char buf[17]; int j = 0;
    for (int i = 60; i >= 0; i -= 4) {
        uint8_t nibble = (n >> i) & 0xF;
        if (nibble) leading = false;
        if (!leading || i == 0) buf[j++] = hex[nibble];
    }
    buf[j] = '\0';
    kprint(buf);
}

// ============================================================
// Boot banner (ASCII art)
// ============================================================
static void printBanner()
{
    kprintCol("  __  __           _    ___  ____  \n", Color::Cyan, VGAColor::Cyan);
    kprintCol(" |  \\/  | ___ _ __| |_ / _ \\/ ___| \n", Color::Cyan, VGAColor::Cyan);
    kprintCol(" | |\\/| |/ _ \\ '__| __| | | \\___ \\ \n", Color::Cyan, VGAColor::Cyan);
    kprintCol(" | |  | |  __/ |  | |_| |_| |___) |\n", Color::Cyan, VGAColor::Cyan);
    kprintCol(" |_|  |_|\\___|_|   \\__|\\___/|____/ \n", Color::Cyan, VGAColor::Cyan);
    kprintCol("\n  MertOS v", Color::Yellow, VGAColor::Yellow);
    kprintCol(MERTOS_VERSION,  Color::Yellow, VGAColor::Yellow);
    kprintCol(" - x86_64 Kernel\n\n", Color::Yellow, VGAColor::Yellow);
}

// ============================================================
// Kernel panic
// ============================================================
[[noreturn]] void kpanic(const char* msg)
{
    asm volatile("cli");
    if (g_fb && g_fb->isReady()) {
        g_fb->consolePrint("\n\n  !!! KERNEL PANIC !!!  \n  ", Color::Red);
        g_fb->consolePrint(msg,                                  Color::White);
        g_fb->consolePrint("\n  System halted.\n",               Color::Red);
    } else if (g_vga) {
        g_vga->setColor(VGAColor::White, VGAColor::Red);
        g_vga->print("\n\n  !!! KERNEL PANIC !!!  \n  ");
        g_vga->print(msg);
        g_vga->print("\n  System halted.\n");
    }
    while (true) asm volatile("hlt");
    __builtin_unreachable();
}

// ============================================================
// Log helpers
// ============================================================
static void logOK(const char* subsystem, const char* msg = nullptr)
{
    kprintCol("[ OK ] ", Color::Green,  VGAColor::Green);
    kprint(subsystem);
    if (msg) { kprint(": "); kprint(msg); }
    kprint("\n");
}

[[maybe_unused]] static void logInfo(const char* msg)
{
    kprintCol("[INFO] ", Color::Cyan, VGAColor::Cyan);
    kprint(msg);
    kprint("\n");
}

// ============================================================
// Statik storage — Heap hazır olana kadar kullanılır
// Heap init sonrası 'new' ile dinamik nesne oluşturulabilir
// ============================================================
alignas(VGADriver)          static char s_vga[sizeof(VGADriver)];
alignas(GDT)                static char s_gdt[sizeof(GDT)];
alignas(IDT)                static char s_idt[sizeof(IDT)];
alignas(DriverManager)      static char s_dm[sizeof(DriverManager)];
alignas(KeyboardDriver)     static char s_kbd[sizeof(KeyboardDriver)];
alignas(MouseDriver)        static char s_mouse[sizeof(MouseDriver)];
alignas(FramebufferDriver)  static char s_fb[sizeof(FramebufferDriver)];
alignas(PMM)                static char s_pmm[sizeof(PMM)];
alignas(VMM)                static char s_vmm[sizeof(VMM)];
alignas(Heap)               static char s_heap[sizeof(Heap)];
alignas(ProcessManager)     static char s_pm[sizeof(ProcessManager)];
alignas(Shell)              static char s_shell[sizeof(Shell)];
alignas(Desktop)            static char s_desktop[sizeof(Desktop)];
alignas(SplashScreen)       static char s_splash[sizeof(SplashScreen)];

// Yeni subsystem statik storage'ları
alignas(VFS)             static char s_vfs[sizeof(VFS)];
alignas(AppManager)      static char s_app_mgr[sizeof(AppManager)];
alignas(EventQueue)      static char s_event_queue[sizeof(EventQueue)];
alignas(Window)          static char s_term_win[sizeof(Window)];
alignas(Window)          static char s_files_win[sizeof(Window)];
alignas(Window)          static char s_settings_win[sizeof(Window)];
alignas(Window)          static char s_msc_win[sizeof(Window)];
alignas(TerminalApp)     static char s_term_app[sizeof(TerminalApp)];
alignas(FileManager)     static char s_files_app[sizeof(FileManager)];
alignas(SettingsApp)     static char s_settings_app_buf[sizeof(SettingsApp)];
alignas(MertStudioCode)  static char s_msc_app_buf[sizeof(MertStudioCode)];

// ============================================================
// IDT → KeyboardDriver köprüsü
// idt.cpp'deki keyboard_handler g_keyboard'a delege eder
// Bu sayede Driver polymorphism'i gerçek anlamda çalışır
// ============================================================
extern "C" void keyboard_handler()
{
    if (g_keyboard) {
        g_keyboard->handleInterrupt(); // Polymorphic çağrı!
    }
    // PIC Master EOI (IRQ1)
    asm volatile("outb %0, %1" : : "a"((uint8_t)0x20), "Nd"((uint16_t)0x20));
}

// Timer tick → ProcessManager + cursor güncelle
extern "C" void timer_handler()
{
    g_timer_ticks++;
    static uint64_t ticks = 0;
    ticks++;
    if (g_processManager) {
        g_processManager->tick();
    }
    // Her tick cursor güncelle (daha akıcı hareket)
    if (g_desktop && g_fb && g_mouse) {
        g_desktop->updateCursor(g_fb, g_mouse->x(), g_mouse->y());
    }
    // PIC Master EOI
    asm volatile("outb %0, %1" : : "a"((uint8_t)0x20), "Nd"((uint16_t)0x20));
}

// Mouse IRQ12 handler
extern "C" void mouse_handler()
{
    if (g_mouse) {
        g_mouse->handleIRQ();
    }
    // Slave PIC EOI (IRQ12 → slave PIC IRQ4)
    asm volatile("outb %0, %1" : : "a"((uint8_t)0x20), "Nd"((uint16_t)0xA0));
    // Master PIC EOI
    asm volatile("outb %0, %1" : : "a"((uint8_t)0x20), "Nd"((uint16_t)0x20));
}

// ============================================================
// kmain — C++ Kernel Giriş Noktası
// entry.asm'den çağrılır.
//   rdi → mb_magic (0x36D76289)
//   rsi → mb2_info pointer
// ============================================================
extern "C" void kmain(uint32_t mb_magic, Mb2InfoHeader* mb2_info)
{
    // ================================================================
    // AŞAMA 1: VGA — Log için en önce başlatılır
    // ================================================================
    VGADriver* vga = new (s_vga) VGADriver();
    g_vga = vga;
    vga->initialize();
    vga->clear();
    printBanner();

    // ================================================================
    // AŞAMA 2: Multiboot2 doğrulama
    // ================================================================
    kprint("Checking Multiboot2 signature... ");
    if (mb_magic != MB2_MAGIC) {
        kprintCol("FAILED\n", Color::Red, VGAColor::Red);
        kpanic("Invalid Multiboot2 magic!");
    }
    logOK("Multiboot2", "signature valid");

    // ================================================================
    // AŞAMA 3: GDT — Segment descriptor'ları kur
    // OOP: GDT sınıfı — Encapsulation
    // ================================================================
    GDT* gdt = new (s_gdt) GDT();
    g_gdt = gdt;
    logOK("GDT", "5 descriptors (kernel/user code+data)");

    // ================================================================
    // AŞAMA 4: IDT — Exception + IRQ handler'ları
    // OOP: IDT sınıfı — Encapsulation
    // ================================================================
    IDT* idt = new (s_idt) IDT();
    g_idt = idt;
    logOK("IDT", "exceptions + IRQ0/IRQ1 armed");

    // ================================================================
    // AŞAMA 5: PMM — Physical Memory Manager
    // OOP: PMM sınıfı — Encapsulation (bitmap private), RAII
    // ================================================================
    // Kernel sınırlarını linker script'ten al
    extern char __bss_end;
    uint64_t kernel_start = 0x100000;
    uint64_t kernel_end   = reinterpret_cast<uint64_t>(&__bss_end);

    PMM* pmm = new (s_pmm) PMM(mb2_info, kernel_start, kernel_end);
    g_pmm = pmm;
    pmm->dumpStats();
    logOK("PMM", "bitmap allocator ready");

    // ================================================================
    // AŞAMA 6: VMM — Virtual Memory Manager
    // OOP: VMM — Composition (PMM kullanır), Encapsulation
    // ================================================================
    VMM* vmm = new (s_vmm) VMM(pmm);
    g_vmm = vmm;

    logOK("VMM", "4-level paging active");

    // Fiziksel adres çözümleme testi
    uint64_t test_phys = vmm->physAddr(0x100000);
    kprintCol("[VMM]  ", Color::Cyan, VGAColor::Cyan);
    kprint("Identity map test: virt=0x100000 -> phys=0x");
    kprintHex(test_phys);
    kprint("\n");
    logOK("VMM", "4-level paging, identity map OK");

    // ================================================================
    // AŞAMA 7: Heap Allocator
    // OOP: Heap sınıfı — Encapsulation (block list private)
    //      RAII — constructor PMM'den sayfa alır, hemen hazır
    //      Composition — Heap HAS-A PMM
    // Heap hazır olduktan sonra: new/delete kullanılabilir
    // ================================================================
    static constexpr uint64_t HEAP_INIT_PAGES = 1024; // 4 MB başlangıç

    Heap* heap = new (s_heap) Heap(pmm, HEAP_INIT_PAGES);
    g_heap = heap;

    if (!heap->validate()) {
        kpanic("Heap initialization failed - structure corrupt!");
    }
    heap->dumpStats();
    logOK("Heap", "4MB, explicit free-list + boundary-tag coalescing");

    // ---- operator new / delete testi ----
    kprintCol("[HEAP] ", Color::Magenta, VGAColor::Magenta);
    kprint("operator new test: ");
    int* test_int = new int(0xDEAD);
    kprint("alloc=0x");
    kprintHex(reinterpret_cast<uint64_t>(test_int));
    kprint(", val=0x");
    kprintHex(*test_int);
    delete test_int;
    kprint(", delete OK\n");

    heap->dumpStats(); // Alloc + free sonrası istatistik

    // ================================================================
    // AŞAMA 8b: Framebuffer Driver (VESA)
    // OOP: FramebufferDriver : public Driver — Inheritance
    // Multiboot2 framebuffer tag'inden adres/pitch/size alınır
    // ================================================================
    FramebufferDriver* fb = new (s_fb) FramebufferDriver();
    g_fb = fb;
    {
        Mb2Parser parser(mb2_info);
        const Mb2TagFramebuffer* fbtag = parser.findFramebuffer();
        if (fbtag && fb->initFromTag(fbtag)) {
            fb->clear(Color::Black);
            fb->setFontScale(2);
            logOK("Framebuffer", "VESA RGB mode active (GRUB tag)");
        } else {
            // GRUB framebuffer tag vermedi — Bochs VBE dogrudan dene
            kprintCol("[FB]   ", Color::Yellow, VGAColor::Yellow);
            kprint("GRUB tag failed, trying Bochs VBE direct init...\n");
            if (fb->initFromBochsVBE(1920, 1080)) {
                fb->clear(Color::Black);
                fb->setFontScale(2);
                logOK("Framebuffer", "Bochs VBE 1920x1080x32 active");
            } else {
                kprintCol("[FB]   ", Color::Yellow, VGAColor::Yellow);
                kprint("No framebuffer - VGA text mode only\n");
            }
        }
    }

    // Boot splash animasyonu
    if (fb->isReady()) {
        SplashScreen* splash = new (s_splash) SplashScreen();
        splash->draw(fb);         // İlk kareyi hemen çiz
        splash->animate(fb);      // 2 saniyelik animasyon + fade
    }

    // ================================================================
    // AŞAMA 9: Driver Manager + Keyboard Driver
    // OOP: DriverManager (Composition), KeyboardDriver : Driver (Inheritance)
    //      virtual initialize() → POLYMORPHISM
    // Heap hazır → artık 'new' ile allocate edebiliriz (statik storage yok)
    // ================================================================
    DriverManager* dm = new (s_dm) DriverManager();
    g_driverManager = dm;

    KeyboardDriver* kbd = new (s_kbd) KeyboardDriver();
    g_keyboard = kbd;

    MouseDriver* mouse = new (s_mouse) MouseDriver();
    g_mouse = mouse;

    dm->registerDriver(vga);
    dm->registerDriver(kbd);
    dm->registerDriver(fb);
    dm->registerDriver(mouse);
    kbd->initialize();
    mouse->initialize();
    mouse->setBounds(fb->width() > 0 ? fb->width() : 1024,
                     fb->height() > 0 ? fb->height() : 768);
    // Cursor başlangıç pozisyonunu mouse ile senkron et
    if (g_desktop) {
        g_desktop->setCursorPos(
            fb->width()  > 0 ? fb->width()  / 2 : 512,
            fb->height() > 0 ? fb->height() / 2 : 384
        );
    }

    // Polymorphism canlı demo: Driver* üzerinden virtual dispatch
    Driver* d = dm->findDriver(Driver::Type::Keyboard);
    if (d) {
        kprintCol("[DRV]  ", Color::Cyan, VGAColor::Cyan);
        kprint("Polymorphic getName(): \"");
        kprint(d->getName());
        kprint("\"\n");
    }
    logOK("Drivers", "VGA + Keyboard registered");

    // ================================================================
    // AŞAMA 10: Process Manager + Scheduler
    // OOP: ProcessManager — Composition (Process nesneleri)
    // ================================================================
    ProcessManager* pm = new (s_pm) ProcessManager();
    g_processManager = pm;
    pm->dumpProcesses();
    logOK("Scheduler", "round-robin, kernel_idle running");

    // ================================================================
    // AŞAMA 11: IPC MessageQueue demo (midterm template kriteri)
    // ================================================================
    MessageQueue<int, 16> test_queue;
    test_queue.send(42);
    test_queue.send(100);
    int val = 0;
    test_queue.receive(val);
    kprintCol("[IPC]  ", Color::Cyan, VGAColor::Cyan);
    kprint("MessageQueue<int,16>: send(42), receive()=");
    kprintDec(val);
    kprint(val == 42 ? "  [PASS]\n" : "  [FAIL]\n");

    // ================================================================
    // AŞAMA 12: Boot tamamlandı
    // ================================================================
    kprint("\n");
    kprintCol("========================================\n", 0x00008888u, VGAColor::DarkCyan);
    kprintCol("  MertOS boot complete!\n",                  Color::White, VGAColor::White);
    kprint("  RAM  : "); kprintDec(pmm->totalBytes() / (1024*1024));
    kprint(" MB total | "); kprintDec(pmm->freeBytes() / (1024*1024));
    kprint(" MB free\n");
    kprint("  Heap : "); kprintDec(heap->totalBytes() / 1024);
    kprint(" KB | used="); kprintDec(heap->usedBytes());
    kprint("B | allocs="); kprintDec(heap->allocCount());
    kprint("\n");
    kprint("  Procs: "); kprintDec(pm->count()); kprint(" running\n");
    kprintCol("========================================\n", 0x00008888u, VGAColor::DarkCyan);
    kprint("\n");

    // ================================================================
    // AŞAMA 8c: Desktop + AppManager + VFS (Framebuffer hazırsa)
    // OOP: Desktop : Widget (INHERITANCE), Window : Widget (INHERITANCE)
    //      AppManager HAS-A App[] (COMPOSITION)
    //      App* → virtual onDraw/onKey/onMouse (POLYMORPHISM)
    //      VFS RAII — constructor / kökünü oluşturur
    // ================================================================
    if (fb->isReady()) {
        // VFS oluştur (RAII — constructor /documents /programs /system yaratır)
        VFS* vfs = new (s_vfs) VFS();
        g_vfs = vfs;
        // Başlangıç dosyaları oluştur
        vfs->mkdir("/documents");
        vfs->mkdir("/programs");
        vfs->mkdir("/system");
        vfs->create("/documents/readme.drk",
            "MertOS v0.3.0\nKastamonu Universitesi\n2024\n");
        vfs->create("/documents/notlar.drk", "OOP Dersi Notlari\n");
        vfs->create("/programs/hello.drk", "print Hello World\n");
        vfs->create("/system/mertos.cfg", "theme=dark\nfont_scale=2\n");
        logOK("VFS", "/documents /programs /system ready");

        // EventQueue
        EventQueue* evq = new (s_event_queue) EventQueue();
        g_event_queue = evq;

        // AppManager
        AppManager* app_mgr = new (s_app_mgr) AppManager();
        g_app_manager = app_mgr;

        // Window boyutları
        int W = fb->width(), H = fb->height();
        int content_y = Desktop::TOPBAR_H;
        int content_h = H - Desktop::TOPBAR_H - Desktop::DOCK_H;

        // Terminal penceresi — ortalanmış, windowed
        int TW = (W * 4) / 5;              // Ekranın %80'i
        int TH = (content_h * 3) / 4;      // İçerik alanının %75'i
        int TX = (W - TW) / 2;             // Yatayda ortalanmış
        int TY = content_y + (content_h - TH) / 2;  // Dikeyde ortalanmış
        Window* term_win = new (s_term_win) Window();
        term_win->setTitle("MertSH  --  MertOS Terminal");
        term_win->setPos(TX, TY);
        term_win->setSize(TW, TH);

        // FileManager penceresi
        Window* files_win = new (s_files_win) Window();
        files_win->setTitle("Dosya Gezgini");
        files_win->setPos(Theme::WIN_MARGIN, content_y + Theme::WIN_MARGIN);
        files_win->setSize(W - Theme::WIN_MARGIN*2, content_h - Theme::WIN_MARGIN*2);
        files_win->setVisible(false);

        // Settings penceresi
        Window* settings_win = new (s_settings_win) Window();
        settings_win->setTitle("Ayarlar");
        settings_win->setPos(50, content_y + 20);
        settings_win->setSize(700, 450);
        settings_win->setVisible(false);

        // MSC penceresi
        Window* msc_win = new (s_msc_win) Window();
        msc_win->setTitle("MertStudio Code");
        msc_win->setPos(Theme::WIN_MARGIN, content_y + Theme::WIN_MARGIN);
        msc_win->setSize(W - Theme::WIN_MARGIN*2, content_h - Theme::WIN_MARGIN*2);
        msc_win->setVisible(false);

        // Apps oluştur
        Shell* shell_ptr = new (s_shell) Shell(vga, kbd);
        g_shell = shell_ptr;

        TerminalApp* term_app = new (s_term_app) TerminalApp(term_win, shell_ptr);
        g_terminal_app = term_app;  // Shell output routing için hemen ayarla
        FileManager* files_app = new (s_files_app) FileManager(files_win, vfs);
        SettingsApp* settings_app = new (s_settings_app_buf) SettingsApp(settings_win);
        MertStudioCode* msc_app = new (s_msc_app_buf) MertStudioCode(msc_win);

        // AppManager'a ekle
        app_mgr->launch(term_app);
        // Files, settings, msc başlangıçta gizli ama kayıtlı
        app_mgr->launch(files_app);
        app_mgr->launch(settings_app);
        app_mgr->launch(msc_app);
        app_mgr->setFocus(0); // Terminal focused

        // Desktop oluştur
        Desktop* desktop = new (s_desktop) Desktop();
        g_desktop = desktop;
        desktop->setPos(0, 0);
        desktop->setSize(W, H);
        desktop->addIcon("Terminal", Theme::Mauve,  'T', 0);
        desktop->addIcon("Dosyalar", Theme::Blue,   'F', 1);
        desktop->addIcon("Ayarlar",  Theme::Teal,   'S', 2);
        desktop->addIcon("MSC",      Theme::Peach,  'M', 3);

        // Process kaydı
        pm->createProcess("mertsh", nullptr, 0);

        // İlk çizim — terminal screen buffer zaten constructor'da dolu
        desktop->draw(fb);
        fb->setFontScale(1);  // TerminalApp kendi scale'ini (1x) yönetiyor
    }

    // ================================================================
    // AŞAMA 13: Ana Event Döngüsü
    // OOP: AppManager Polymorphism dispatch — App::onKey() virtual çağrı
    //      EventQueue Template → MessageQueue<Event,64> kullanımı
    // ================================================================
    logOK("EventLoop", "desktop event dispatch active");

    // Mouse tracking
    uint8_t prev_mouse_buttons = 0;
    int     prev_mouse_x = 0, prev_mouse_y = 0;

    // Ana döngü — hiç dönmez
    while (true) {
        // Klavye event'lerini işle → focused app'e dispatch
        KeyEvent ke;
        while (g_keyboard && g_keyboard->getEvent(ke)) {
            if (ke.pressed) {
                // Alt+F4: aktif pencereyi kapat
                if (ke.alt && ke.code == KeyCode::F4) {
                    if (g_app_manager && g_app_manager->focused()) {
                        App* fa = g_app_manager->focused();
                        if (fa->window()) fa->window()->setVisible(false);
                        if (g_desktop && g_fb) g_desktop->draw(g_fb);
                    }
                }
                // Ctrl+Tab: focus next app
                else if (ke.ctrl && ke.code == KeyCode::TAB) {
                    if (g_app_manager) {
                        g_app_manager->focusNext();
                        if (g_desktop && g_fb) g_desktop->draw(g_fb);
                    }
                } else {
                    if (g_app_manager) g_app_manager->dispatchKey(ke);
                }
            }
        }

        // Mouse event işle
        if (g_mouse) {
            uint8_t cur_btn = g_mouse->buttons();
            int     mx      = g_mouse->x();
            int     my      = g_mouse->y();
            bool    lmb_now  = (cur_btn  & 0x01) != 0;
            bool    lmb_prev = (prev_mouse_buttons & 0x01) != 0;

            if (lmb_now && !lmb_prev) {
                // Sol tuş: yeni tıklandı → click (drag başlatabilir)
                if (g_desktop) g_desktop->handleClick(mx, my);
            } else if (lmb_now && lmb_prev) {
                // Tuş basılı + mouse hareket → drag
                if ((mx != prev_mouse_x || my != prev_mouse_y) && g_desktop) {
                    g_desktop->handleMouseMove(mx, my);
                }
            } else if (!lmb_now && lmb_prev) {
                // Sol tuş bırakıldı → drag sona erdi
                if (g_desktop) g_desktop->handleMouseRelease(mx, my);
            }

            prev_mouse_buttons = cur_btn;
            prev_mouse_x       = mx;
            prev_mouse_y       = my;
        }

        asm volatile("hlt");
    }
}
