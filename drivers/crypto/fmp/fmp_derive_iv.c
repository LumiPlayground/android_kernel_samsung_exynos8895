/*
 * Exynos FMP derive iv for eCryptfs
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/crypto.h>
#include <crypto/hash.h>
#include <linux/module.h>
#include <linux/scatterlist.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/fs.h>

#define FMP_MAX_IV_BYTES		16
#define FMP_MAX_OFFSET_BYTES		16
#define DEFAULT_HASH			"md5"
#define SHA256_HASH			"sha256-fmp"

/* Hash transforms are allocated before storage userspace starts; I/O uses
 * only stack descriptors and never allocates while holding host locks. */
static struct crypto_shash *fmp_md5_tfm;
#ifdef CONFIG_CRYPTO_FIPS
static struct crypto_shash *fmp_sha256_tfm;
#endif

static int __init fmp_iv_hash_init(void)
{
	fmp_md5_tfm = crypto_alloc_shash(DEFAULT_HASH, 0, 0);
	if (IS_ERR(fmp_md5_tfm))
		return PTR_ERR(fmp_md5_tfm);
#ifdef CONFIG_CRYPTO_FIPS
	fmp_sha256_tfm = crypto_alloc_shash(SHA256_HASH, 0, 0);
	if (IS_ERR(fmp_sha256_tfm)) {
		crypto_free_shash(fmp_md5_tfm);
		fmp_md5_tfm = NULL;
		return PTR_ERR(fmp_sha256_tfm);
	}
#endif
	return 0;
}
module_init(fmp_iv_hash_init);

static int fmp_iv_digest(struct crypto_shash *tfm, char *dst, char *src, int len)
{
	if (IS_ERR_OR_NULL(tfm))
		return -ENODEV;
	{
		SHASH_DESC_ON_STACK(desc, tfm);
		int ret;
		desc->tfm = tfm;
		desc->flags = 0;
		ret = crypto_shash_digest(desc, src, len, dst);
		shash_desc_zero(desc);
		return ret;
	}
}

#ifdef CONFIG_CRYPTO_FIPS
int calculate_sha256(struct crypto_shash *tfm, char *dst, char *src, int len)
{
	return fmp_iv_digest(tfm ? tfm : fmp_sha256_tfm, dst, src, len);
}
#endif

int calculate_md5(struct crypto_shash *tfm, char *dst, char *src, int len)
{
	return fmp_iv_digest(tfm ? tfm : fmp_md5_tfm, dst, src, len);
}

int file_enc_derive_iv(struct address_space *mapping, loff_t offset, char *extent_iv)
{
	char src[FMP_MAX_IV_BYTES + FMP_MAX_OFFSET_BYTES];
	int ret;

	memcpy(src, mapping->iv, FMP_MAX_IV_BYTES);
	memset(src + FMP_MAX_IV_BYTES, 0, FMP_MAX_OFFSET_BYTES);
	snprintf(src + FMP_MAX_IV_BYTES, FMP_MAX_OFFSET_BYTES, "%lld", offset);

#ifdef CONFIG_CRYPTO_FIPS
	if (mapping->cc_enable)
		ret = calculate_sha256(mapping->hash_tfm, extent_iv, src, FMP_MAX_IV_BYTES + FMP_MAX_OFFSET_BYTES);
	else
#endif
		ret = calculate_md5(mapping->hash_tfm, extent_iv, src, FMP_MAX_IV_BYTES + FMP_MAX_OFFSET_BYTES);
	if (ret) {
		printk(KERN_ERR "%s: Error attempting to compute generating IV for a page; ret = [%d]\n", __func__, ret);
		goto out;
	}

out:
	return ret;
}

