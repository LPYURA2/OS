#include <styleos/desktop_v0.h>
#include <styleos/arch.h>

static inline void out8(uint16_t port, uint8_t value)
{
    __asm__ volatile ("outb %0,%1" :: "a"(value), "Nd"(port));
}
static inline uint8_t in8(uint16_t port)
{
    uint8_t v;
    __asm__ volatile ("inb %1,%0" : "=a"(v) : "Nd"(port));
    return v;
}
static inline void out16(uint16_t port, uint16_t value)
{
    __asm__ volatile ("outw %0,%1" :: "a"(value), "Nd"(port));
}
static inline uint16_t in16(uint16_t port)
{
    uint16_t v;
    __asm__ volatile ("inw %1,%0" : "=a"(v) : "Nd"(port));
    return v;
}

enum { KDATA = 0x60, KSTAT = 0x64, POLLS = 60000 };
static bool ps_write(uint16_t port, uint8_t byte)
{
    for (unsigned i = 0; i < POLLS; ++i) {
        uint8_t status = in8(KSTAT);
        if (status != 0xff && !(status & 2)) {
            out8(port, byte);
            return true;
        }
    }
    return false;
}

static bool ps_read(bool mouse, uint8_t *byte)
{
    for (unsigned i = 0; i < POLLS; ++i) {
        uint8_t s = in8(KSTAT);
        if (s == 0xff || !(s & 1)) continue;
        uint8_t b = in8(KDATA);
        if (((s & 0x20) != 0) == mouse && !(s & 0xc0)) {
            *byte = b;
            return true;
        }
    }
    return false;
}

static bool ps_device(bool mouse, uint8_t command)
{
    for (unsigned attempt = 0; attempt < 2; ++attempt) {
        uint8_t ack;
        if ((mouse && !ps_write(KSTAT, 0xd4)) || !ps_write(KDATA, command) ||
            !ps_read(mouse, &ack)) return false;
        if (ack == 0xfa) return true;
        if (ack != 0xfe) return false;
    }
    return false;
}

static uint8_t packet[3], packet_len, pause_bytes, modifiers;
static bool extended, shift_l, shift_r, ctrl_l, ctrl_r, alt_l, alt_r;

unsigned desk_input_init(void)
{
    uint8_t config;
    if (!ps_write(KSTAT, 0xad) || !ps_write(KSTAT, 0xa7)) return 0;
    for (unsigned i = 0; i < 64 && (in8(KSTAT) & 1); ++i) (void)in8(KDATA);
    if (!ps_write(KSTAT, 0x20) || !ps_read(false, &config) ||
        !ps_write(KSTAT, 0x60) || !ps_write(KDATA, (config & ~3u) | 0x40u)) return 0;
    bool key = ps_write(KSTAT, 0xae) && ps_device(false, 0xf5) &&
               ps_device(false, 0xf0) && ps_device(false, 2) && ps_device(false, 0xf4);
    bool mouse = ps_write(KSTAT, 0xa8) && ps_device(true, 0xf6) && ps_device(true, 0xf4);
    return (key ? 1u : 0u) | (mouse ? 2u : 0u);
}

bool desk_input_next(struct desk_event *event)
{
    for (unsigned i = 0; i < 24; ++i) {
        uint8_t status = in8(KSTAT);
        if (status == 0xff || !(status & 1)) return false;
        uint8_t b = in8(KDATA);
        if (status & 0xc0) {
            if (status & 0x20) packet_len = 0;
            else { extended = false; pause_bytes = 0; }
            continue;
        }
        if (status & 0x20) {
            if (!packet_len && !(b & 8)) continue;
            packet[packet_len++] = b;
            if (packet_len != 3) continue;
            packet_len = 0;
            int dx = packet[1] - ((packet[0] & 0x10) ? 256 : 0);
            int dy = packet[2] - ((packet[0] & 0x20) ? 256 : 0);
            *event = (struct desk_event){.mouse = true, .dx = dx, .dy = -dy,
                                         .buttons = packet[0] & 7, .modifiers = modifiers};
            return true;
        }
        if (pause_bytes) { --pause_bytes; continue; }
        if (b == 0xe1) { pause_bytes = 5; extended = false; continue; }
        if (b == 0xe0) { extended = true; continue; }
        bool e = extended;
        extended = false;
        bool down = !(b & 0x80);
        unsigned key = b & 0x7f;
        if (!e && key == 0x2a) shift_l = down;
        else if (!e && key == 0x36) shift_r = down;
        else if (key == 0x1d) { if (e) ctrl_r = down; else ctrl_l = down; }
        else if (key == 0x38) { if (e) alt_r = down; else alt_l = down; }
        else if (down) {
            *event = (struct desk_event){.key = key | (e ? 0x100 : 0), .modifiers = modifiers};
            return true;
        }
        modifiers = ((shift_l || shift_r) ? DESK_SHIFT : 0) |
                    ((ctrl_l || ctrl_r) ? DESK_CTRL : 0) |
                    ((alt_l || alt_r) ? DESK_ALT : 0);
    }
    return false;
}

void desk_reboot(void) { (void)ps_write(KSTAT, 0xfe); }

static uint8_t rtc(unsigned n) { out8(0x70, (uint8_t)n); return in8(0x71); }
static unsigned bcd(unsigned b) { return (b >> 4) * 10 + (b & 15); }
bool desk_clock(unsigned *hour, unsigned *minute)
{
    if (rtc(0x0a) & 0x80) return false;
    unsigned m = rtc(2), h = rtc(4), b = rtc(0x0b);
    if ((rtc(0x0a) & 0x80) || m != rtc(2) || h != rtc(4)) return false;
    bool pm = h & 0x80;
    h &= 0x7f;
    if (!(b & 4)) { h = bcd(h); m = bcd(m); }
    if (!(b & 2)) h = (h % 12) + (pm ? 12 : 0);
    if (h > 23 || m > 59) return false;
    *hour = h;
    *minute = m;
    return true;
}

enum { ATA = 0x170, ALT = 0x376, BSY = 0x80, DRQ = 8, DF = 0x20, ERR = 1 };
static uint32_t sectors;
static void ata_fault(const char *where)
{
    unsigned status = in8(ATA + 7), error = in8(ATA + 1);
    unsigned report = status << 8 | error;
    arch_puts("STYLEOS DESKTOP ATA "); arch_puts(where); arch_puts(" 0x");
    for (int shift = 12; shift >= 0; shift -= 4) arch_putc("0123456789abcdef"[(report >> shift) & 15]);
    arch_putc('\n');
}

static void ata_delay(void) { for (unsigned i = 0; i < 4; ++i) (void)in8(ALT); }
static bool ata_wait(bool data, bool error_check)
{
    for (unsigned i = 0; i < 300000; ++i) {
        uint8_t s = in8(ATA + 7);
        if (!s || s == 0xff) return false;
        if (s & BSY) continue;
        if (error_check && (s & (ERR | DF))) return false;
        if (!!(s & DRQ) == data) return true;
    }
    return false;
}

uint32_t desk_ata_probe(void)
{
    sectors = 0;
    out8(ALT, 2); /* Disable this channel's interrupts. */
    out8(ATA + 6, 0xa0); /* Secondary channel, master. */
    ata_delay();
    if (!ata_wait(false, false)) { arch_puts("STYLEOS DESKTOP ATA NOT READY\n"); return 0; }
    for (unsigned i = 2; i <= 5; ++i) out8(ATA + i, 0);
    out8(ATA + 7, 0xec);
    ata_delay();
    if (!ata_wait(true, true) || in8(ATA + 4) || in8(ATA + 5)) {
        arch_puts("STYLEOS DESKTOP ATA IDENTIFY REFUSED\n"); return 0;
    }
    uint16_t w[256];
    for (unsigned i = 0; i < 256; ++i) w[i] = in16(ATA);
    if (!ata_wait(false, true) || !(w[49] & (1u << 9)) ||
        (w[83] & 0xc000u) != 0x4000u || !(w[83] & (1u << 12))) {
        arch_puts("STYLEOS DESKTOP ATA CAPABILITY REFUSED\n"); return 0;
    }
    if ((w[106] & 0xd000u) == 0x5000u && (w[117] != 256 || w[118])) return 0;
    sectors = (uint32_t)w[60] | ((uint32_t)w[61] << 16);
    arch_puts("STYLEOS DESKTOP ATA SECTORS 0x");
    for (int shift = 28; shift >= 0; shift -= 4) arch_putc("0123456789abcdef"[(sectors >> shift) & 15]);
    arch_putc('\n');
    return sectors;
}

static bool ata_start(uint32_t lba, uint8_t command)
{
    if (lba >= sectors || lba >= 0x10000000u || !ata_wait(false, false)) return false;
    out8(ATA + 6, 0xe0 | (uint8_t)(lba >> 24));
    ata_delay();
    out8(ATA + 1, 0);
    out8(ATA + 2, 1);
    out8(ATA + 3, (uint8_t)lba);
    out8(ATA + 4, (uint8_t)(lba >> 8));
    out8(ATA + 5, (uint8_t)(lba >> 16));
    out8(ATA + 7, command);
    ata_delay();
    return ata_wait(true, true);
}

bool desk_ata_read(uint32_t lba, uint8_t data[512])
{
    if (!ata_start(lba, 0x20)) { ata_fault("READ START ERROR"); return false; }
    for (unsigned i = 0; i < 512; i += 2) {
        uint16_t word = in16(ATA);
        data[i] = (uint8_t)word;
        data[i + 1] = (uint8_t)(word >> 8);
    }
    ata_delay();
    if (!ata_wait(false, true)) { ata_fault("READ FINISH ERROR"); return false; }
    return true;
}

bool desk_ata_write(uint32_t lba, const uint8_t data[512])
{
    if (!ata_start(lba, 0x30)) return false;
    for (unsigned i = 0; i < 512; i += 2) out16(ATA, data[i] | ((uint16_t)data[i + 1] << 8));
    ata_delay();
    return ata_wait(false, true);
}

bool desk_ata_flush(void)
{
    if (!sectors || !ata_wait(false, false)) return false;
    out8(ATA + 7, 0xe7);
    ata_delay();
    return ata_wait(false, true);
}
