// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Intel Corporation <www.intel.com>
 *
 */

#include <common.h>
#include <hang.h>
#include <asm/arch/mailbox_s10.h>
#include <asm/arch/secure_vab.h>
#include <asm/arch/smc_api.h>
#include <asm/unaligned.h>
#include <exports.h>
#include <image.h>
#include <linux/intel-smc.h>
#include <log.h>

#define CHUNKSZ_PER_WD_RESET		(256 * 1024)

/*
 * Read the length of the VAB certificate from the end of image
 * and calculate the actual image size (excluding the VAB certificate).
 */
static size_t get_img_size(u8 *img_buf, size_t img_buf_sz)
{
	u8 *img_buf_end = img_buf + img_buf_sz;
	u32 cert_sz = get_unaligned_le32(img_buf_end - sizeof(u32));
	u8 *p = img_buf_end - cert_sz - sizeof(u32);

	/* Ensure p is pointing within the img_buf */
	if (p < img_buf || p > (img_buf_end - VAB_CERT_HEADER_SIZE))
		return 0;

	if (get_unaligned_le32(p) == SDM_CERT_MAGIC_NUM)
		return (size_t)(p - img_buf);

	return 0;
}

void board_fit_image_post_process(void **p_image, size_t *p_size)
{
	int retry_count = 20;
	u8 hash384[SHA384_SUM_LEN];
	u64 img_addr, mbox_data_addr;
	size_t img_sz, mbox_data_sz;
	u8 *cert_hash_ptr;
	u32 backup_word;
	u32 resp = 0, resp_len = 1;
	int ret;

	img_addr = (uintptr_t)*p_image;

	debug("Authenticating image at address 0x%016llx (%ld bytes)\n",
	      img_addr, *p_size);

	img_sz = get_img_size((u8 *)img_addr, *p_size);
	debug("img_sz = %ld\n", img_sz);

	if (!img_sz) {
		puts("VAB certificate not found in image!\n");
		goto fail;
	}

	if (!IS_ALIGNED(img_sz, sizeof(u32))) {
		printf("Image size (%ld bytes) not aliged to 4 bytes!\n",
		       img_sz);
		goto fail;
	}

	/* Generate HASH384 from the image */
	sha384_csum_wd((u8 *)img_addr, img_sz, hash384, CHUNKSZ_PER_WD_RESET);

	cert_hash_ptr = (u8 *)(img_addr + img_sz + VAB_CERT_MAGIC_OFFSET +
			       VAB_CERT_FIT_SHA384_OFFSET);

	/*
	 * Compare the SHA384 found in certificate against the SHA384
	 * calculated from image
	 */
	if (memcmp(hash384, cert_hash_ptr, SHA384_SUM_LEN)) {
		puts("SHA384 not match!\n");
		goto fail;
	}

	mbox_data_addr = img_addr + img_sz - sizeof(u32);
	/* Size in word (32bits) */
	mbox_data_sz = (ALIGN(*p_size - img_sz, 4)) >> 2;

	debug("mbox_data_addr = 0x%016llx\n", mbox_data_addr);
	debug("mbox_data_sz = %ld\n", mbox_data_sz);

	/* We need to use the 4 bytes before the certificate for T */
	backup_word = *(u32 *)mbox_data_addr;
	/* T = 0 */
	*(u32 *)mbox_data_addr = 0;

	do {
#if !defined(CONFIG_SPL_BUILD) && defined(CONFIG_SPL_ATF)
		/* Invoke SMC call to ATF to send the VAB certificate to SDM */
		ret  = smc_send_mailbox(MBOX_VAB_SRC_CERT, mbox_data_sz,
					(u32 *)mbox_data_addr, 0, &resp_len,
					&resp);
#else
		/* Send the VAB certficate to SDM for authentication */
		ret = mbox_send_cmd(MBOX_ID_UBOOT, MBOX_VAB_SRC_CERT,
				    MBOX_CMD_DIRECT, mbox_data_sz,
				    (u32 *)mbox_data_addr, 0, &resp_len,
				    &resp);
#endif
		/* If SDM is not available, just delay 50ms and retry again */
		if (ret == MBOX_RESP_DEVICE_BUSY)
			mdelay(50);
		else
			break;
	} while (--retry_count);

	/* Restore the original 4 bytes */
	*(u32 *)mbox_data_addr = backup_word;

	/* Exclude the size of the VAB certificate from image size */
	*p_size = img_sz;

	debug("ret = 0x%08x, resp = 0x%08x, resp_len = %d\n", ret, resp,
	      resp_len);

	if (ret) {
		/*
		 * Unsupported mailbox command or device not in the
		 * owned/secure state
		 */
		if (ret == MBOX_RESP_UNKNOWN ||
		    ret == MBOX_RESP_NOT_ALLOWED_UNDER_SECURITY_SETTINGS) {
			/* SDM bypass authentication */
			printf("Image Authentication bypassed at address "
			       "0x%016llx (%ld bytes)\n", img_addr, img_sz);
			return;
		}
		puts("VAB certificate authentication failed in SDM");
		if (ret == MBOX_RESP_DEVICE_BUSY)
			puts("(SDM busy timeout)");
		puts("\n");
		goto fail;
	} else {
		/* If Certificate Process Status has error */
		if (resp)
			goto fail;
	}

	debug("Image Authentication passed\n");

	return;

fail:
	printf("Image Authentication failed at address 0x%016llx "
	       "(%ld bytes)\n", img_addr, img_sz);
	hang();
}

#ifndef CONFIG_SPL_BUILD
void board_prep_linux(bootm_headers_t *images)
{
	/*
	 * Ensure the OS is always booted from FIT and with
	 * VAB signed certificate
	 */
	if (!images->fit_uname_cfg) {
		printf("Please use FIT with VAB signed images!\n");
		hang();
	}
}
#endif
