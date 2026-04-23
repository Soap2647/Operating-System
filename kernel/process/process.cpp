// process.cpp — ProcessManager implementasyonu

#include "process.hpp"
#include "../memory/pmm.hpp"
#include "../drivers/vga.hpp"

extern "C" void process_entry_wrapper();

ProcessManager* g_processManager = nullptr;

// ============================================================
// Yardımcı: String kopyala
// ============================================================
void ProcessManager::copyName(char* dst, const char* src, int maxLen)
{
    int i = 0;
    while (i < maxLen - 1 && src[i]) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

// ============================================================
// Constructor — RAII
// Kernel idle process'i (PID 0) oluşturur
// ============================================================
ProcessManager::ProcessManager()
    : m_current(nullptr), m_count(0), m_next_pid(0), m_ticks(0)
{
    // Tüm process slot'larını boş işaretle
    for (int i = 0; i < MAX_PROCESSES; i++) {
        m_processes[i].m_state = ProcessState::UNUSED;
        m_processes[i].m_pid   = 0;
    }

    // PID 0: Kernel Idle Process
    // Bu process scheduler'da çalışacak process bulamadığında çalışır.
    // Entry fonksiyonu: sonsuz hlt döngüsü
    uint32_t idle_pid = createProcess("kernel_idle",
        []() { while(true) asm volatile("hlt"); }, 255); // En düşük öncelik

    if (idle_pid) {
        m_current = getByPID(idle_pid);
        if (m_current) m_current->m_state = ProcessState::RUNNING;
    }
}

// ============================================================
// findFreeSlot
// ============================================================
Process* ProcessManager::findFreeSlot()
{
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (m_processes[i].m_state == ProcessState::UNUSED) {
            return &m_processes[i];
        }
    }
    return nullptr;
}

// ============================================================
// createProcess — Yeni kernel process oluştur
// NASIL:
//   1. Boş slot bul
//   2. PMM'den kernel stack sayfaları ayır
//   3. CpuContext'i ayarla (RSP = stack_top, RIP = entry)
//   4. State → READY
// ============================================================
uint32_t ProcessManager::createProcess(const char* name,
                                        Process::EntryFunc entry,
                                        uint32_t priority)
{
    if (!g_pmm) return 0;

    Process* proc = findFreeSlot();
    if (!proc) return 0;

    // Kernel stack için sayfa ayır
    uint64_t stack_phys = g_pmm->allocPages(Process::KERNEL_STACK_PAGES);
    if (!stack_phys) return 0;

    uint64_t stack_top = stack_phys + Process::KERNEL_STACK_SIZE;

    // Process alanını hazırla
    proc->m_pid          = m_next_pid++;
    proc->m_state        = ProcessState::CREATED;
    proc->m_priority     = priority;
    proc->m_time_slice   = DEFAULT_TIME_SLICE;
    proc->m_sleep_ticks  = 0;
    proc->m_kernel_stack = stack_phys;
    proc->m_stack_top    = stack_top;
    copyName(proc->m_name, name, 32);

    // CpuContext ayarla:
    // Stack'e entry fonksiyon adresini koy
    // switchContext → ret → process_entry_wrapper → entry()
    uint64_t* stack_ptr = reinterpret_cast<uint64_t*>(stack_top);

    // Stack'e process_entry_wrapper adresini push (return address gibi)
    *(--stack_ptr) = reinterpret_cast<uint64_t>(process_entry_wrapper);

    // Context: RSP bu fake return adresini gösteriyor
    // RAX'a entry adresi koyuyoruz (process_entry_wrapper bunu çağırır)
    proc->m_context = {};
    proc->m_context.rsp = reinterpret_cast<uint64_t>(stack_ptr);
    proc->m_context.rbx = reinterpret_cast<uint64_t>(entry); // entry adresi

    // CR3: mevcut kernel address space'ini kullan (kernel process)
    uint64_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    proc->m_context.cr3 = cr3;

    // RAX'ı context'e koy — process_entry_wrapper rbx'i kullanacak
    // rbx = entry func pointer olarak ayarlandı yukarıda

    m_count++;
    return proc->m_pid;
}

// ============================================================
// schedule — Round-Robin Scheduler
// Bir sonraki READY process'i seç
// ============================================================
Process* ProcessManager::schedule()
{
    if (!m_current) return nullptr;

    // Mevcut process'in index'ini bul
    int current_idx = 0;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (&m_processes[i] == m_current) {
            current_idx = i;
            break;
        }
    }

    // Round-robin: current'dan sonra READY olanı bul
    for (int i = 1; i <= MAX_PROCESSES; i++) {
        int idx = (current_idx + i) % MAX_PROCESSES;
        Process* p = &m_processes[idx];

        if (p->m_state == ProcessState::READY ||
            p->m_state == ProcessState::CREATED) {
            return p;
        }
    }

    // Hiç READY process yok → idle'a dön
    return m_current; // Aynı process devam eder
}

// ============================================================
// contextSwitch — Scheduler karar verdi, switch yap
// ============================================================
void ProcessManager::contextSwitch(Process* next)
{
    if (!next || next == m_current) return;

    Process* prev = m_current;

    // State güncellemeleri
    if (prev->m_state == ProcessState::RUNNING)
        prev->m_state = ProcessState::READY;

    next->m_state = ProcessState::RUNNING;
    m_current     = next;

    // Assembly context switch
    switchContext(&prev->m_context, &next->m_context);
    // Bu noktada next process çalışıyor
}

// ============================================================
// tick — Timer IRQ'dan çağrılır
// ============================================================
void ProcessManager::tick()
{
    m_ticks++;

    // Uyuyan process'leri uyandır
    for (int i = 0; i < MAX_PROCESSES; i++) {
        Process* p = &m_processes[i];
        if (p->m_state == ProcessState::SLEEPING && p->m_sleep_ticks > 0) {
            if (--p->m_sleep_ticks == 0) {
                p->m_state = ProcessState::READY;
            }
        }
    }

    if (!m_current) return;

    // Time slice tükenmiş mi?
    if (m_current->m_time_slice > 0) {
        m_current->m_time_slice--;
        return;
    }

    // Time slice bitti → schedule
    m_current->m_time_slice = DEFAULT_TIME_SLICE; // Reset
    Process* next = schedule();
    if (next && next != m_current) {
        contextSwitch(next);
    }
}

// ============================================================
// Diğer işlemler
// ============================================================
void ProcessManager::killProcess(uint32_t pid)
{
    Process* p = getByPID(pid);
    if (!p) return;
    p->m_state = ProcessState::ZOMBIE;
    // Stack sayfalarını PMM'e geri ver
    if (g_pmm && p->m_kernel_stack) {
        g_pmm->freePages(p->m_kernel_stack, Process::KERNEL_STACK_PAGES);
    }
    m_count--;
}

void ProcessManager::blockProcess(uint32_t pid)
{
    Process* p = getByPID(pid);
    if (p && p->m_state == ProcessState::RUNNING)
        p->m_state = ProcessState::BLOCKED;
}

void ProcessManager::wakeProcess(uint32_t pid)
{
    Process* p = getByPID(pid);
    if (p && p->m_state == ProcessState::BLOCKED)
        p->m_state = ProcessState::READY;
}

void ProcessManager::sleepProcess(uint32_t pid, uint32_t ticks)
{
    Process* p = getByPID(pid);
    if (!p) return;
    p->m_sleep_ticks = ticks;
    p->m_state       = ProcessState::SLEEPING;
}

Process* ProcessManager::getByPID(uint32_t pid)
{
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (m_processes[i].m_state != ProcessState::UNUSED &&
            m_processes[i].m_pid == pid) {
            return &m_processes[i];
        }
    }
    return nullptr;
}

void ProcessManager::dumpProcesses() const
{
    if (!g_vga) return;
    const char* state_names[] = {
        "UNUSED", "CREATED", "RUNNING", "READY",
        "BLOCKED", "SLEEPING", "ZOMBIE"
    };
    g_vga->printColored("[PROC] Process Table:\n", VGAColor::Cyan);
    for (int i = 0; i < MAX_PROCESSES; i++) {
        const Process& p = m_processes[i];
        if (p.m_state == ProcessState::UNUSED) continue;
        g_vga->print("  PID=");
        g_vga->printDec(p.m_pid);
        g_vga->print(" [");
        g_vga->print(state_names[static_cast<int>(p.m_state)]);
        g_vga->print("] ");
        g_vga->print(p.m_name);
        g_vga->print("\n");
    }
}
