#ifndef _LINUX_BOOT_CHECKPOINT_H
#define _LINUX_BOOT_CHECKPOINT_H

#include <linux/types.h>

#ifdef CONFIG_EXYNOS8895_BOOT_CHECKPOINTS
void boot_checkpoint(unsigned int row, const char *fmt, ...) __printf(2, 3);
void boot_checkpoint_fb_init(void);
void boot_checkpoint_linear(void);
bool boot_checkpoint_display_active(void);
bool boot_checkpoint_reserved(phys_addr_t start, size_t size);
void boot_checkpoint_syscall(bool exit, long nr, long ret, unsigned long pc);
#else
static inline void boot_checkpoint(unsigned int row, const char *fmt, ...) {}
static inline void boot_checkpoint_fb_init(void) {}
static inline void boot_checkpoint_linear(void) {}
static inline bool boot_checkpoint_display_active(void) { return false; }
static inline bool boot_checkpoint_reserved(phys_addr_t start, size_t size)
{
	return false;
}
static inline void boot_checkpoint_syscall(bool exit, long nr, long ret,
					   unsigned long pc) {}
#endif

#endif
