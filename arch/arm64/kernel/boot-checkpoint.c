/* Temporary bringup display: use the bootloader's existing linear RGB scanout. */
#include <linux/boot_checkpoint.h>
#include <linux/font.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/initrd.h>
#include <linux/kernel.h>
#include <linux/memblock.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <asm/cacheflush.h>
#include <asm/early_ioremap.h>
#include <asm/memory.h>
#include <asm/sections.h>

#define ROWS 60
#define COLS 76
#define DECON 0x12860000
#define IDMA 0x128b1000

static char lines[ROWS][COLS];
static DEFINE_RAW_SPINLOCK(checkpoint_lock);
static phys_addr_t fb_phys;
static unsigned int stride, width, height, scale, format;
static bool linear, active;

static u32 __init early_reg(phys_addr_t addr)
{
	void __iomem *p = early_ioremap(addr, 4);
	u32 value = 0;

	if (p) {
		value = readl(p);
		early_iounmap(p, 4);
	}
	return value;
}

static u32 __init early_pte(phys_addr_t addr)
{
	void *p;
	u32 value = 0;

	if (!memblock_is_region_memory(addr, 4))
		return 0;
	p = early_memremap(addr, 4);
	if (p) {
		value = READ_ONCE(*(u32 *)p);
		early_memunmap(p, 4);
	}
	return value;
}

/* Exynos SysMMU uses 36-bit physical addresses encoded with a four-bit shift. */
static phys_addr_t __init scanout_phys(u32 addr, phys_addr_t table)
{
	u32 pte;

	if (!table)
		return addr;
	pte = early_pte(table + (addr >> 20) * 4);
	if ((pte & 7) == 2)
		return ((phys_addr_t)(pte & ~0xffffU) << 4) | (addr & 0xfffff);
	if ((pte & 7) != 1)
		return 0;
	pte = early_pte(((phys_addr_t)(pte & ~0x3fU) << 4) +
			((addr >> 12) & 255) * 4);
	if ((pte & 3) == 2)
		return ((phys_addr_t)(pte & ~0xffU) << 4) | (addr & 0xfff);
	if ((pte & 3) == 1)
		return ((phys_addr_t)(pte & ~0xfffU) << 4) | (addr & 0xffff);
	return 0;
}

static void __ref draw_row(unsigned int row)
{
	unsigned int x, y, bit, ch, px, py, pitch = 18 * scale;
	unsigned int y0 = 24 + row * pitch;
	size_t bytes = (size_t)stride * pitch * 4;
	phys_addr_t addr = fb_phys + (size_t)y0 * stride * 4;
	u32 *pixels;
	u32 black = format == 2 || format == 3 || format == 6 || format == 7 ?
		0x000000ff : 0xff000000;

	if (!active || y0 + pitch > height)
		return;
	pixels = linear ? phys_to_virt(addr) : early_memremap(addr, bytes);
	if (!pixels)
		return;
	for (y = 0; y < pitch; y++)
		for (x = 0; x < width; x++)
			pixels[y * stride + x] = black;
	for (ch = 0; ch < COLS - 1 && lines[row][ch]; ch++) {
		if (24 + (ch + 1) * 8 * scale > width)
			break;
		for (y = 0; y < 16; y++) {
			u8 glyph = ((const u8 *)font_vga_8x16.data)[(u8)lines[row][ch] * 16 + y];
			for (bit = 0; bit < 8; bit++) {
				if (!(glyph & (0x80 >> bit)))
					continue;
				for (py = 0; py < scale; py++)
					for (px = 0; px < scale; px++)
						pixels[(y * scale + py) * stride +
							24 + (ch * 8 + bit) * scale + px] = ~0U;
			}
		}
	}
	__flush_dcache_area(pixels, bytes);
	if (!linear)
		early_memunmap(pixels, bytes);
}

void boot_checkpoint(unsigned int row, const char *fmt, ...)
{
	unsigned long flags;
	va_list ap;

	if (row >= ROWS || !raw_spin_trylock_irqsave(&checkpoint_lock, flags))
		return;
	snprintf(lines[row], COLS, "%02u ", row);
	va_start(ap, fmt);
	vsnprintf(lines[row] + 3, COLS - 3, fmt, ap);
	va_end(ap);
	draw_row(row);
	raw_spin_unlock_irqrestore(&checkpoint_lock, flags);
}

void __init boot_checkpoint_fb_init(void)
{
	u32 windows, channels, channel, win, cfg, src, img, offset, dma;
	phys_addr_t table = 0, mmu, p, end;
	size_t bytes;
	void __iomem *trigger;

	if (!(early_reg(DECON) & BIT(4)))
		goto unavailable;
	windows = early_reg(DECON + 0x214);
	channels = early_reg(DECON + 0x218);
	for (win = 0; win < 6; win++)
		if ((windows & (3U << (win * 4))) == (1U << (win * 4)))
			break;
	if (win == 6)
		goto reference;
	channel = (channels >> (win * 4)) & 7;
	if (channel > 5)
		goto unavailable;
	dma = IDMA + channel * 0x1000;
	cfg = early_reg(dma + 8);
	format = (cfg >> 11) & 31;
	if (format > 7 || (cfg & (BIT(7) | (3 << 8))) ||
		(early_reg(dma) & BIT(0)))
		goto unavailable;
	src = early_reg(dma + 0x10);
	img = early_reg(dma + 0x18);
	offset = early_reg(dma + 0x14);
	stride = src & 0xffff;
	width = img & 0x1fff;
	height = (img >> 16) & 0x1fff;
	if (offset || width < 720 || width > 4096 || height < 1280 ||
		height > 4096 || stride < width || stride > 1536 ||
		height > ((src >> 16) & 0x3fff))
		goto reference;
	scale = width >= 1280 ? 2 : 1;
	bytes = (size_t)stride * height * 4;
	mmu = channel < 2 ? (channel ? 0x12920000 : 0x12910000) :
		(channel < 4 ? (channel == 2 ? 0x12910000 : 0x12920000) :
		 0x12900000);
	if (early_reg(mmu) & BIT(0))
		table = (phys_addr_t)early_reg(mmu + 0xc) << 12;
	dma = early_reg(dma + 0x40);
	if (!dma || (dma & ~PAGE_MASK) || (u64)dma + bytes > (1ULL << 32))
		goto reference;
	fb_phys = scanout_phys(dma, table);
	goto validate;
reference:
	/* Dreamlte bootloader layout supplied by the board bringup reference. */
	fb_phys = 0xcc000000;
	dma = fb_phys;
	table = 0;
	width = stride = 1440;
	height = 2960;
	scale = 2;
	format = 0;
	channel = 0xff;
	bytes = (size_t)stride * height * 4;
validate:
	end = fb_phys + PAGE_ALIGN(bytes);
	if (!fb_phys || !memblock_is_region_memory(fb_phys, PAGE_ALIGN(bytes)))
		goto unavailable;
	/* Never treat an IOVA as RAM, or write into kernel/initrd allocations. */
	if (fb_phys < __pa_symbol(_end) && end > __pa_symbol(_text))
		goto unavailable;
	if (initrd_start && fb_phys < __virt_to_phys(initrd_end) &&
		end > __virt_to_phys(initrd_start))
		goto unavailable;
	for (p = fb_phys; p < end; p += PAGE_SIZE)
		if (!memblock_is_reserved(p) || !memblock_is_map_memory(p) ||
			scanout_phys(dma + p - fb_phys, table) != p)
			goto unavailable;
	active = true;
	for (win = 0; win < ROWS; win++)
		if (lines[win][0])
			draw_row(win);
	boot_checkpoint(0, "BOOT CHECKPOINTS %ux%u CH%u", width, height, channel);
	boot_checkpoint(1, "RGB BUFFER %llx FORMAT %u", (u64)fb_phys, format);
	/* Keep the running bootloader command-mode scanout refreshing on TE. */
	trigger = early_ioremap(DECON + 0x70, 4);
	if (trigger) {
		writel(0x1281, trigger);
		early_iounmap(trigger, 4);
	}
	pr_info("boot-checkpoint: scanout %pa %ux%u stride %u channel %u\n",
		&fb_phys, width, height, stride, channel);
	return;
unavailable:
	pr_warn("boot-checkpoint: unsupported bootloader scanout; RAM not modified\n");
}

void __init boot_checkpoint_linear(void)
{
	linear = true;
}

bool boot_checkpoint_display_active(void)
{
	return active;
}

bool boot_checkpoint_reserved(phys_addr_t start, size_t size)
{
	return active && start < fb_phys + (size_t)stride * height * 4 &&
		start + size > fb_phys;
}

void boot_checkpoint_syscall(bool exit, long nr, long ret, unsigned long pc)
{
	static unsigned long enters, exits;
	unsigned long n;

	if (task_pid_nr(current) != 1)
		return;
	n = exit ? ++exits : ++enters;
	if (n > 64 && (n & 1023))
		return;
	boot_checkpoint(exit ? 48 : 47, "PID1 %s NR=%ld RET=%ld COUNT=%lu",
		exit ? "EXIT" : "ENTER", nr, ret, n);
	boot_checkpoint(49, "PID1 USER PC %016lx", pc);
}
