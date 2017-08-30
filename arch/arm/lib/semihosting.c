/*
 * Copyright 2014 Broadcom Corporation
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

/*
 * Minimal semihosting implementation for reading files into memory. If more
 * features like writing files or console output are required they can be
 * added later. This code has been tested on arm64/aarch64 fastmodel only.
 * An untested placeholder exists for armv7 architectures, but since they
 * are commonly available in silicon now, fastmodel usage makes less sense
 * for them.
 */
#include <common.h>
#include <command.h>
#include <asm/semihosting.h>

#define SYSOPEN		0x01
#define SYSCLOSE	0x02
#define SYSWRITEC	0x03
#define SYSWRITE0	0x04
#define SYSREAD		0x06
#define SYSREADC	0x07
#define SYSFLEN		0x0C

#define MODE_READ	0x0
#define MODE_READBIN	0x1

void __auto_semihosting(void) __attribute__((alias("smh_puts")));

/*
 * Call the handler
 */
static int smh_trap(unsigned int sysnum, void *addr)
{
#if defined(CONFIG_ARM64)
	register int result asm("r0");
	asm volatile ("hlt #0xf000" : "=r" (result) : "0"(sysnum), "r"(addr));
#else
#ifdef CONFIG_SYS_THUMB_BUILD
	int result;
	asm volatile ("mov r0, %1; mov r1, %2; svc 0xAB; mov %0, r0"
#else
	asm volatile ("mov r0, %1; mov r1, %2; svc 0x123456; mov %0, r0"
#endif
		: "=r" (result)
		: "r" (sysnum), "r" (addr)
		: "r0", "r1", "r2", "r3", "ip", "lr", "memory", "cc");

#endif
	return result;
}

/*
 * Open, load a file into memory, and close it. Check that the available space
 * is sufficient to store the entire file. Return the bytes actually read from
 * the file as seen by the read function. The verbose flag enables some extra
 * printing of successful read status.
 */
int smh_load(const char *fname, void *memp, int avail, int verbose)
{
	int ret, fd, len;

	ret = -1;

	debug("%s: fname \'%s\', avail %u, memp %p\n", __func__, fname,
	      avail, memp);

	/* Open the file */
	fd = smh_open(fname, "rb");
	if (fd == -1)
		return ret;

	/* Get the file length */
	ret = smh_len_fd(fd);
	if (ret == -1) {
		smh_close(fd);
		return ret;
	}

	/* Check that the file will fit in the supplied buffer */
	if (ret > avail) {
		printf("%s: ERROR ret %d, avail %u\n", __func__, ret,
		       avail);
		smh_close(fd);
		return ret;
	}

	len = ret;

	/* Read the file into the buffer */
	ret = smh_read(fd, memp, len);
	if (ret == 0) {
		/* Print successful load information if requested */
		if (verbose) {
			printf("\n%s\n", fname);
			printf("    0x%8p dest\n", memp);
			printf("    0x%08x size\n", len);
			printf("    0x%08x avail\n", avail);
		}
	}

	/* Close the file */
	smh_close(fd);

	return ret;
}

/*
 * Read 'len' bytes of file into 'memp'. Returns 0 on success, else failure
 */
int smh_read(int fd, void *memp, int len)
{
	int ret;
	struct smh_read_s {
		int fd;
		void *memp;
		int len;
	} read;

	debug("%s: fd %d, memp %p, len %d\n", __func__, fd, memp, len);

	read.fd = fd;
	read.memp = memp;
	read.len = len;

	ret = smh_trap(SYSREAD, &read);
	if (ret == 0) {
		return 0;
	} else {
		/*
		 * The ARM handler allows for returning partial lengths,
		 * but in practice this never happens so rather than create
		 * hard to maintain partial read loops and such, just fail
		 * with an error message.
		 */
		printf("%s: ERROR ret %d, fd %d, len %u memp %p\n",
		       __func__, ret, fd, len, memp);
	}
	return ret;
}

/*
 * Open a file on the host. Mode is "r" or "rb" currently. Returns a file
 * descriptor or -1 on error.
 */
int smh_open(const char *fname, char *modestr)
{
	int ret, fd, mode;
	struct smh_open_s {
		const char *fname;
		unsigned int mode;
		unsigned int len;
	} open;

	debug("%s: file \'%s\', mode \'%s\'\n", __func__, fname, modestr);

	ret = -1;

	/* Check the file mode */
	if (!(strcmp(modestr, "r"))) {
		mode = MODE_READ;
	} else if (!(strcmp(modestr, "rb"))) {
		mode = MODE_READBIN;
	} else {
		printf("%s: ERROR mode \'%s\' not supported\n", __func__,
		       modestr);
		return ret;
	}

	open.fname = fname;
	open.len = strlen(fname);
	open.mode = mode;

	/* Open the file on the host */
	fd = smh_trap(SYSOPEN, &open);
	if (fd == -1)
		printf("%s: ERROR fd %d for file \'%s\'\n", __func__, fd,
		       fname);

	return fd;
}

/*
 * Close the file using the file descriptor
 */
int smh_close(int fd)
{
	int ret;
	long fdlong;

	debug("%s: fd %d\n", __func__, fd);

	fdlong = (long)fd;
	ret = smh_trap(SYSCLOSE, &fdlong);
	if (ret == -1)
		printf("%s: ERROR fd %d\n", __func__, fd);

	return ret;
}

/*
 * Get the file length from the file descriptor
 */
int smh_len_fd(int fd)
{
	int ret;
	long fdlong;

	debug("%s: fd %d\n", __func__, fd);

	fdlong = (long)fd;
	ret = smh_trap(SYSFLEN, &fdlong);
	if (ret == -1)
		printf("%s: ERROR ret %d\n", __func__, ret);

	return ret;
}

/*
 * Get the file length from the filename
 */
int smh_len(const char *fname)
{
	int ret, fd, len;

	debug("%s: file \'%s\'\n", __func__, fname);

	/* Open the file */
	fd = smh_open(fname, "rb");
	if (fd == -1)
		return fd;

	/* Get the file length */
	len = smh_len_fd(fd);

	/* Close the file */
	ret = smh_close(fd);
	if (ret == -1)
		return ret;

	debug("%s: returning len %d\n", __func__, len);

	/* Return the file length (or -1 error indication) */
	return len;
}

/* perform a write to console buffer */
int smh_puts(const char *buffer)
{
	return smh_trap(SYSWRITE0, (void *)buffer);
}
int smh_putc(const char c)
{
	return smh_trap(SYSWRITEC, (void *)&c);
}

int smh_getc(void)
{
	return smh_trap(SYSREADC, NULL);
}

static int smh_load_file(const char * const name, ulong load_addr,
			 ulong *end_addr)
{
	long fd;
	long len;
	long ret;

	fd = smh_open(name, "rb");
	if (fd == -1)
		return -1;

	len = smh_len_fd(fd);
	if (len < 0) {
		smh_close(fd);
		return -1;
	}

	ret = smh_read(fd, (void *)load_addr, len);
	smh_close(fd);

	if (ret == 0) {
		*end_addr = load_addr + len - 1;
		printf("loaded file %s from %08lX to %08lX, %08lX bytes\n",
		       name,
		       load_addr,
		       *end_addr,
		       len);
	} else {
		printf("read failed\n");
		return 0;
	}

	return 0;
}

static int do_smhload(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	if (argc == 3 || argc == 4) {
		ulong load_addr;
		ulong end_addr = 0;
		int ret;
		char end_str[64];

		load_addr = simple_strtoul(argv[2], NULL, 16);
		if (!load_addr)
			return -1;

		ret = smh_load_file(argv[1], load_addr, &end_addr);
		if (ret < 0)
			return CMD_RET_FAILURE;

		/* Optionally save returned end to the environment */
		if (argc == 4) {
			sprintf(end_str, "0x%08lx", end_addr);
			setenv(argv[3], end_str);
		}
	} else {
		return CMD_RET_USAGE;
	}
	return 0;
}

U_BOOT_CMD(smhload, 4, 0, do_smhload, "load a file using semihosting",
	   "<file> 0x<address> [end var]\n"
	   "    - load a semihosted file to the address specified\n"
	   "      if the optional [end var] is specified, the end\n"
	   "      address of the file will be stored in this environment\n"
	   "      variable.\n");
