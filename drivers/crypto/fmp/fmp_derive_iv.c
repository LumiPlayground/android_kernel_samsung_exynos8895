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
#include <linux/scatterlist.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/fs.h>

#define FMP_MAX_IV_BYTES		16
#define FMP_MAX_OFFSET_BYTES		16
#define DEFAULT_HASH			"md5"
#define SHA256_HASH			"sha256-fmp"

static int calculate_hash(struct crypto_shash *tfm, char *dst,
			  char *src, int len)
{
	/* The encrypted mapping owns a preallocated transform. */
	if (!tfm || len < 0)
		return -EINVAL;
	{
		SHASH_DESC_ON_STACK(desc, tfm);
		int ret;

		desc->tfm = tfm;
		desc->flags = 0;
		ret = crypto_shash_digest(desc, (u8 *)src, len, (u8 *)dst);
		memzero_explicit(desc, sizeof(*desc) + crypto_shash_descsize(tfm));
		return ret;
	}
}

int calculate_sha256(struct crypto_shash *hash_tfm, char *dst, char *src, int len)
{
	return calculate_hash(hash_tfm, dst, src, len);
}

int calculate_md5(struct crypto_shash *hash_tfm, char *dst, char *src, int len)
{
	return calculate_hash(hash_tfm, dst, src, len);
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

