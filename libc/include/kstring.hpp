// kstring.hpp — Kernel String Utility Library
// STL / libc yok — kendi implementasyonlarımız
// Tüm fonksiyonlar inline: her .cpp'ye header include yeterli

#pragma once
#include <stdint.h>

// ============================================================
// Uzunluk & Karşılaştırma
// ============================================================
inline int kstrlen(const char* s)
{
    if (!s) return 0;
    int n = 0;
    while (s[n]) n++;
    return n;
}

inline int kstrcmp(const char* a, const char* b)
{
    while (*a && *b && *a == *b) { a++; b++; }
    return static_cast<unsigned char>(*a) - static_cast<unsigned char>(*b);
}

inline int kstrncmp(const char* a, const char* b, int n)
{
    while (n-- > 0 && *a && *b) {
        if (*a != *b) return static_cast<unsigned char>(*a) - static_cast<unsigned char>(*b);
        a++; b++;
    }
    return 0;
}

// Büyük/küçük harf duyarsız karşılaştırma
inline int kstricmp(const char* a, const char* b)
{
    auto lower = [](char c) -> char {
        return (c >= 'A' && c <= 'Z') ? c + 32 : c;
    };
    while (*a && *b && lower(*a) == lower(*b)) { a++; b++; }
    return static_cast<unsigned char>(lower(*a)) - static_cast<unsigned char>(lower(*b));
}

// ============================================================
// Kopyalama
// ============================================================
inline char* kstrcat(char* dst, const char* src)
{
    char* d = dst;
    while (*d) d++;           // dst'nin sonuna git
    while ((*d++ = *src++));  // src'yi ekle
    return dst;
}

inline char* kstrcpy(char* dst, const char* src)
{
    char* d = dst;
    while ((*d++ = *src++));
    return dst;
}

inline char* kstrncpy(char* dst, const char* src, int n)
{
    int i = 0;
    while (i < n - 1 && src[i]) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
    return dst;
}

// ============================================================
// Arama
// ============================================================
inline const char* kstrchr(const char* s, char c)
{
    while (*s) { if (*s == c) return s; s++; }
    return (c == '\0') ? s : nullptr;
}

inline const char* kstrstr(const char* haystack, const char* needle)
{
    int nl = kstrlen(needle);
    if (nl == 0) return haystack;
    while (*haystack) {
        if (kstrncmp(haystack, needle, nl) == 0) return haystack;
        haystack++;
    }
    return nullptr;
}

// ============================================================
// Dönüşüm
// ============================================================

// int64 → string (base 10 veya 16)
inline char* kitoa(int64_t n, char* buf, int base = 10)
{
    if (n == 0) { buf[0] = '0'; buf[1] = '\0'; return buf; }

    const char* digits = "0123456789ABCDEF";
    bool negative = (n < 0 && base == 10);
    uint64_t val = negative ? static_cast<uint64_t>(-n) : static_cast<uint64_t>(n);

    char tmp[66];
    int  i = 0;
    while (val > 0) { tmp[i++] = digits[val % base]; val /= base; }
    if (negative) tmp[i++] = '-';

    int j = 0;
    while (i-- > 0) buf[j++] = tmp[i];
    buf[j] = '\0';
    return buf;
}

// uint64 → hex string
inline char* khtoa(uint64_t n, char* buf, bool prefix = true)
{
    const char* hex = "0123456789ABCDEF";
    char tmp[17];
    int  i = 0;
    if (n == 0) { tmp[i++] = '0'; }
    else { while (n > 0) { tmp[i++] = hex[n & 0xF]; n >>= 4; } }

    int j = 0;
    if (prefix) { buf[j++] = '0'; buf[j++] = 'x'; }
    while (i-- > 0) buf[j++] = tmp[i];
    buf[j] = '\0';
    return buf;
}

// string → int64 (basit, sadece decimal ve hex "0x..." destekler)
inline int64_t katoi(const char* s)
{
    if (!s || !*s) return 0;
    bool neg = false;
    if (*s == '-') { neg = true; s++; }

    int64_t result = 0;
    int base = 10;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) { base = 16; s += 2; }

    while (*s) {
        int digit = 0;
        if (*s >= '0' && *s <= '9')      digit = *s - '0';
        else if (*s >= 'a' && *s <= 'f') digit = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'F') digit = *s - 'A' + 10;
        else break;
        result = result * base + digit;
        s++;
    }
    return neg ? -result : result;
}

// ============================================================
// Boşluk / Trim
// ============================================================
inline bool kisspace(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

inline bool kisdigit(char c) { return c >= '0' && c <= '9'; }
inline bool kisalpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
inline bool kisalnum(char c) { return kisdigit(c) || kisalpha(c); }

// Başındaki boşlukları atla → ilk non-space pointer
inline const char* kskipws(const char* s)
{
    while (kisspace(*s)) s++;
    return s;
}

// Satır sonu \n'den önce gelen \r ve \n'leri kırp (in-place)
inline void krtrim(char* s)
{
    int n = kstrlen(s);
    while (n > 0 && (s[n-1] == '\n' || s[n-1] == '\r' || s[n-1] == ' '))
        s[--n] = '\0';
}

// ============================================================
// Bellek İşlemleri
// ============================================================
inline void* kmemset(void* dst, int val, uint64_t n)
{
    uint8_t* d = reinterpret_cast<uint8_t*>(dst);
    while (n--) *d++ = static_cast<uint8_t>(val);
    return dst;
}

inline void* kmemcpy(void* dst, const void* src, uint64_t n)
{
    uint8_t*       d = reinterpret_cast<uint8_t*>(dst);
    const uint8_t* s = reinterpret_cast<const uint8_t*>(src);
    while (n--) *d++ = *s++;
    return dst;
}

inline int kmemcmp(const void* a, const void* b, uint64_t n)
{
    const uint8_t* pa = reinterpret_cast<const uint8_t*>(a);
    const uint8_t* pb = reinterpret_cast<const uint8_t*>(b);
    while (n--) {
        if (*pa != *pb) return *pa - *pb;
        pa++; pb++;
    }
    return 0;
}
