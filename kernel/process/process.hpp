// process.hpp — Process ve ProcessManager sınıfları
// OOP: Encapsulation (process state), Composition (ProcessManager has Process[])
// Midterm: ProcessManager kriteri

#pragma once
#include <stdint.h>

// ============================================================
// CPU Register Context — Context Switch için
// Tüm callee-saved register'lar + RSP burada saklanır.
// entry.asm benzeri: bu struct'ı stack'e push/pop olarak düşün.
// ============================================================
struct CpuContext {
    // Callee-saved registers (System V AMD64 ABI)
    uint64_t r15, r14, r13, r12;
    uint64_t rbx;
    uint64_t rbp;
    // Stack pointer — switch sonrası iret buradan devam eder
    uint64_t rsp;
    // Sayfa tablosu (her process kendi address space'ine sahip)
    uint64_t cr3;
} __attribute__((packed));

// ============================================================
// Process Durumları
// ============================================================
enum class ProcessState : uint8_t {
    UNUSED   = 0,   // Slot boş
    CREATED  = 1,   // Oluşturuldu, henüz çalışmadı
    RUNNING  = 2,   // Şu an CPU'da
    READY    = 3,   // Çalışmaya hazır, kuyrukta bekliyor
    BLOCKED  = 4,   // I/O veya event bekliyor
    SLEEPING = 5,   // Timer bekliyor
    ZOMBIE   = 6,   // Tamamlandı, parent wait bekleniyor
};

// ============================================================
// Process Sınıfı
// OOP: Encapsulation — state, stack, context private
//       ProcessManager friend sınıf
// ============================================================
class ProcessManager; // Forward declaration

class Process {
    friend class ProcessManager; // Scheduler register'lara erişebilir

public:
    using EntryFunc = void(*)();

    static constexpr uint32_t KERNEL_STACK_PAGES = 4;   // 16 KB kernel stack
    static constexpr uint32_t KERNEL_STACK_SIZE  = KERNEL_STACK_PAGES * 4096;

    // ---- Sorgulama (public) ----
    uint32_t      getPID()      const { return m_pid;   }
    ProcessState  getState()    const { return m_state; }
    const char*   getName()     const { return m_name;  }
    uint32_t      getPriority() const { return m_priority; }
    bool          isRunnable()  const {
        return m_state == ProcessState::READY ||
               m_state == ProcessState::CREATED;
    }

private:
    uint32_t     m_pid;           // Process ID
    char         m_name[32];      // Process adı (debug için)
    ProcessState m_state;
    uint32_t     m_priority;      // Round-robin için (0 = en yüksek)
    uint32_t     m_time_slice;    // Kalan tick sayısı
    uint32_t     m_sleep_ticks;   // Uyku sayacı

    CpuContext   m_context;       // CPU register durumu

    uint64_t     m_kernel_stack;  // Kernel stack adresi
    uint64_t     m_stack_top;     // Stack'in üst sınırı
};

// ============================================================
// ProcessManager Sınıfı
// OOP: Composition (Process nesneleri yönetir)
//      Scheduler logic burada
// ============================================================
class ProcessManager {
public:
    static constexpr int     MAX_PROCESSES = 32;
    static constexpr uint32_t DEFAULT_TIME_SLICE = 10; // tick

    // RAII: Constructor ilk "kernel idle" process'i oluşturur
    ProcessManager();

    // ---- Process Yönetimi ----

    // Yeni kernel process oluştur
    // entry: process'in çalıştıracağı fonksiyon
    // Döner: PID (0 = hata)
    uint32_t createProcess(const char* name, Process::EntryFunc entry,
                           uint32_t priority = 10);

    // Process'i sonlandır
    void killProcess(uint32_t pid);

    // Process'i bloke et (I/O bekliyor)
    void blockProcess(uint32_t pid);

    // Process'i uyanık et
    void wakeProcess(uint32_t pid);

    // Belirtilen tick kadar uyut
    void sleepProcess(uint32_t pid, uint32_t ticks);

    // ---- Scheduler ----

    // Bir sonraki çalışacak process'i seç (round-robin)
    Process* schedule();

    // Context switch gerçek: current → next
    // Assembly implementasyonu: context_switch.asm
    void contextSwitch(Process* next);

    // Timer tick: time_slice azalt, gerekirse schedule
    // Timer IRQ'dan çağrılır
    void tick();

    // ---- Sorgulama ----
    Process* current()  const { return m_current; }
    Process* getByPID(uint32_t pid);
    int      count()    const { return m_count; }

    // Debug: process listesini VGA'ya yaz
    void dumpProcesses() const;

private:
    Process  m_processes[MAX_PROCESSES];
    Process* m_current;   // Şu an çalışan process
    int      m_count;     // Toplam process sayısı
    uint32_t m_next_pid;  // PID sayacı
    uint64_t m_ticks;     // Toplam tick sayısı

    // Boş process slot bul
    Process* findFreeSlot();

    // Process'in adını kopyala (strcpy yerine)
    static void copyName(char* dst, const char* src, int maxLen);
};

extern ProcessManager* g_processManager;

// ============================================================
// Context switch fonksiyonu — context_switch.asm'de tanımlı
// void switchContext(CpuContext* old_ctx, CpuContext* new_ctx)
// ============================================================
extern "C" void switchContext(CpuContext* old_ctx, CpuContext* new_ctx);
