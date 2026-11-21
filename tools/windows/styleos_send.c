/* Portable x64 Windows sender. No administrator rights, disk access other
 * than reading the chosen file, network, or installed runtime required. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef void *HANDLE;
typedef uint16_t WCHAR;
typedef uint32_t DWORD;
typedef int32_t BOOL;
typedef int64_t LARGE_INTEGER;
typedef const WCHAR *LPCWSTR;
typedef WCHAR *LPWSTR;
#define API __declspec(dllimport)
#define BAD_HANDLE ((HANDLE)(intptr_t)-1)
#define IO_PENDING 997
#define SIZE_LIMIT (1024u * 1024u)
#define CHUNK 12u

struct selection {
    DWORD size;
    HANDLE hwnd, instance;
    LPCWSTR filter;
    LPWSTR custom_filter;
    DWORD max_custom, index;
    LPWSTR file;
    DWORD max_file;
    LPWSTR file_title;
    DWORD max_file_title;
    LPCWSTR directory, title;
    DWORD flags;
    uint16_t file_offset, extension_offset;
    LPCWSTR default_ext;
    uintptr_t custom_data;
    void *hook;
    LPCWSTR template_name;
    void *reserved;
    DWORD reserved_flags, flags_ex;
};

struct overlapped {
    uintptr_t internal, internal_high;
    DWORD offset, offset_high;
    HANDLE event;
};

_Static_assert(sizeof(struct selection) == 152, "OPENFILENAMEW x64");
_Static_assert(sizeof(struct overlapped) == 32, "OVERLAPPED x64");

API BOOL GetOpenFileNameW(struct selection *);
API int MessageBoxW(HANDLE, LPCWSTR, LPCWSTR, unsigned);
API HANDLE CreateFileW(LPCWSTR, DWORD, DWORD, void *, DWORD, DWORD, HANDLE);
API BOOL GetFileSizeEx(HANDLE, LARGE_INTEGER *);
API BOOL ReadFile(HANDLE, void *, DWORD, DWORD *, struct overlapped *);
API BOOL WriteFile(HANDLE, const void *, DWORD, DWORD *, struct overlapped *);
API BOOL CloseHandle(HANDLE);
API BOOL WaitNamedPipeW(LPCWSTR, DWORD);
API BOOL PeekNamedPipe(HANDLE, void *, DWORD, DWORD *, DWORD *, DWORD *);
API HANDLE CreateEventW(void *, BOOL, BOOL, LPCWSTR);
API BOOL ResetEvent(HANDLE);
API BOOL GetOverlappedResultEx(HANDLE, struct overlapped *, DWORD *, DWORD, BOOL);
API BOOL CancelIoEx(HANDLE, struct overlapped *);
API DWORD GetLastError(void);
API _Noreturn void ExitProcess(unsigned);

static uint8_t payload[SIZE_LIMIT];
static WCHAR filename[32768];
static struct overlapped operation;
static uint8_t reply;
static const WCHAR pipe_name[] = u"\\\\.\\pipe\\styleos-updates";
static const WCHAR title[] = u"Передача обновления StyleOS";
static struct selection dialog = {
    .size = sizeof(dialog),
    .filter = u"Обновление StyleOS (*.supdate)\0*.supdate\0",
    .index = 1, .file = filename, .max_file = sizeof(filename) / sizeof(*filename),
    .title = u"Выберите пакет обновления StyleOS",
    .flags = 0x00080000 | 0x00001000 | 0x00000800 | 0x00000008,
    .default_ext = u"supdate"
};

static bool io(HANDLE channel, void *bytes, DWORD count, bool send)
{
    DWORD done = 0;
    if (!ResetEvent(operation.event)) return false;
    operation.internal = operation.internal_high = 0;
    BOOL started = send ? WriteFile(channel, bytes, count, &done, &operation) :
                          ReadFile(channel, bytes, count, &done, &operation);
    if (!started && GetLastError() != IO_PENDING) return false;
    if (started) return done == count;
    if (GetOverlappedResultEx(channel, &operation, &done, 7000, 0)) return done == count;
    (void)CancelIoEx(channel, &operation);
    return false;
}

static bool frame(HANDLE channel, void *bytes, DWORD count)
{
    return io(channel, bytes, count, true) && io(channel, &reply, 1, false) && reply == 6;
}

static bool transmit(DWORD size)
{
    if (!WaitNamedPipeW(pipe_name, 3000)) return false;
    HANDLE channel = CreateFileW(pipe_name, 0xc0000000u, 0, 0, 3, 0x40110000, 0); /* Overlapped, identification only. */
    if (channel == BAD_HANDLE) return false;
    operation.event = CreateEventW(0, 1, 0, 0);
    if (!operation.event) { (void)CloseHandle(channel); return false; }
    /* Static buffers stay valid even if an overlapped operation is cancelled. */
    static uint8_t header[12] = "STYHOST1";
    for (unsigned i = 0; i < 4; ++i) header[8 + i] = (uint8_t)(size >> (8 * i));
    bool ok = true;
    /* Ignore bounded startup output from firmware before the first request. */
    static uint8_t stale[256];
    for (unsigned i = 0; ok && i < 64; ++i) {
        DWORD available = 0;
        ok = PeekNamedPipe(channel, 0, 0, 0, &available, 0);
        if (!available) break;
        ok = ok && io(channel, stale, available < sizeof(stale) ? available : sizeof(stale), false);
    }
    ok = ok && frame(channel, header, sizeof(header));
    for (DWORD off = 0; ok && off < size; off += CHUNK)
        ok = frame(channel, payload + off, size - off < CHUNK ? size - off : CHUNK);
    (void)CloseHandle(channel);
    (void)CloseHandle(operation.event);
    return ok;
}

void start(void)
{
    if (!GetOpenFileNameW(&dialog)) ExitProcess(0);
    HANDLE file = CreateFileW(filename, 0x80000000u, 1, 0, 3, 0x80, 0);
    LARGE_INTEGER size = 0;
    DWORD read = 0;
    bool ok = file != BAD_HANDLE && GetFileSizeEx(file, &size) &&
              size >= 640 && size <= SIZE_LIMIT &&
              ReadFile(file, payload, (DWORD)size, &read, 0) && read == (DWORD)size;
    if (file != BAD_HANDLE) (void)CloseHandle(file);
    if (!ok) {
        (void)MessageBoxW(0, u"Не удалось прочитать пакет. Выберите файл .supdate до 1 МиБ.", title, 0x10);
        ExitProcess(1);
    }
    if (!transmit(read)) {
        (void)MessageBoxW(0, reply == 21 ? u"StyleOS отказала в приёме. Проверьте сообщение в Проводнике." :
                    u"Нет ответа от StyleOS. Проверьте COM2 и нажмите «С ПК» в папке «Обновления».",
                    title, 0x10);
        ExitProcess(1);
    }
    (void)MessageBoxW(0, u"Пакет сохранён в папке «Обновления». Установите его из StyleOS.", title, 0x40);
    ExitProcess(0);
}
