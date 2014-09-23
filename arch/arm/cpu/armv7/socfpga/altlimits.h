/*
 * Copyright (C) 2014 Altera Corporation. All rights reserved
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * This is an abstract header file for best portability. uBoot does not
 * have the equivalent of a limits.h file (Linux has kernel.h), and
 * some recently added code requires range checks on computation results.
 *
 * This code first checks for the existence of the macro before defining it
 * in the event limits.h or the equivalent is ever added in the future.
 * This file can be removed if that ever occurs.
 */

#ifndef __ALTLIMITS_H__
#define __ALTLIMITS_H__

/*
 * ANSI C - number of bits by type
 */
#ifndef CHAR_BIT
	#define CHAR_BIT	(8)
#endif

#ifndef BYTE_BIT
	#define BYTE_BIT	(8)
#endif

#ifndef SHORT_BIT
	#define SHORT_BIT	(16)
#endif

#ifndef INT_BIT
	#define INT_BIT		(32)
#endif

#ifndef BYTE_MAX
	#define BYTE_MAX	(255)
#endif

#ifndef SCHAR_MAX
	#define SCHAR_MAX	(127)
	#define SCHAR_MIN	(-SCHAR_MAX - 1)
#endif

#ifndef CHAR_MAX
	#define CHAR_MAX	(127)
	#define CHAR_MIN	(-CHAR_MAX - 1)
#endif

#ifndef UCHAR_MAX
	#define UCHAR_MAX	(0xff)
#endif

#ifndef SHRT_MAX
	#define SHRT_MAX	(32767)
	#define SHRT_MIN	(-SHRT_MAX - 1)
#endif

#ifndef USHRT_MAX
	#define USHRT_MAX	(0xffff)
#endif

#ifndef INT_MAX
	#define INT_MAX		(2147483647)
	#define INT_MIN		(-INT_MAX - 1)
#endif

#ifndef UINT_MAX
	#define UINT_MAX	(0xffffffff)
#endif

#ifndef LONG_MAX
	#define LONG_MAX	(INT_MAX)
	#define LONG_MIN	(-LONG_MAX - 1)
#endif

#ifndef ULONG_MAX
	#define ULONG_MAX	(UINT_MAX)
#endif

#ifndef INT8_MIN
	#define INT8_MIN	(SCHAR_MIN)
#endif

#ifndef INT8_MAX
	#define INT8_MAX	(SCHAR_MAX)
#endif

#ifndef UINT8_MAX
	#define UINT8_MAX	(UCHAR_MAX)
#endif

#ifndef INT16_MIN
	#define INT16_MIN	(SHRT_MIN)
#endif

#ifndef INT16_MAX
	#define INT16_MAX	(SHRT_MAX)
#endif

#ifndef UINT16_MAX
	#define UINT16_MAX	(USHRT_MAX)
#endif

#ifndef INT32_MIN
	#define INT32_MIN	(INT_MIN)
#endif

#ifndef INT32_MAX
	#define INT32_MAX	(INT_MAX)
#endif

#ifndef UINT32_MAX
	#define UINT32_MAX	(UINT_MAX)
#endif

#endif /* __ALTLIMITS_H__ */


