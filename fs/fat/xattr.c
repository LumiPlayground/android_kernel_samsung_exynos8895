#include <linux/file.h>
#include <linux/fs.h>
#include <linux/xattr.h>
#include <linux/dcache.h>
#include "fat.h"

#ifndef CONFIG_FAT_VIRTUAL_XATTR_SELINUX_LABEL
#define CONFIG_FAT_VIRTUAL_XATTR_SELINUX_LABEL	("undefined")
#endif

static const char default_xattr[] = CONFIG_FAT_VIRTUAL_XATTR_SELINUX_LABEL;

int fat_setxattr(struct dentry *dentry, struct inode *inode, const char *name, const void *value, size_t size, int flags) {
	if (!name || strcmp(name, "security.selinux"))
		return -EOPNOTSUPP;
	return 0;
}

ssize_t fat_getxattr(struct dentry *dentry, struct inode *inode, const char *name, void *value, size_t size) {
	if (!name || strcmp(name, "security.selinux"))
		return -EOPNOTSUPP;
	if (size > strlen(default_xattr)+1 && value)
		strcpy(value, default_xattr);
	return strlen(default_xattr);
}

ssize_t fat_listxattr(struct dentry *dentry, char *list, size_t size) {
	return 0;
}

int fat_removexattr(struct dentry *dentry, const char *name) {
	if (!name || strcmp(name, "security.selinux"))
		return -EOPNOTSUPP;
	return 0;
}

static int fat_xattr_get(const struct xattr_handler *handler,
		struct dentry *dentry, struct inode *inode,
		const char *name, void *value, size_t size)
{
	return fat_getxattr(dentry, inode, XATTR_NAME_SELINUX, value, size);
}

static int fat_xattr_set(const struct xattr_handler *handler,
		struct dentry *dentry, struct inode *inode,
		const char *name, const void *value, size_t size, int flags)
{
	if (!value)
		return fat_removexattr(dentry, XATTR_NAME_SELINUX);
	return fat_setxattr(dentry, inode, XATTR_NAME_SELINUX, value, size, flags);
}

static const struct xattr_handler fat_xattr_handler = {
	.name = XATTR_NAME_SELINUX,
	.get = fat_xattr_get,
	.set = fat_xattr_set,
};

const struct xattr_handler *fat_xattr_handlers[] = {
	&fat_xattr_handler,
	NULL
};
