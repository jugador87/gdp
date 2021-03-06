/* vim: set ai sw=8 sts=8 ts=8 : */

/***********************************************************************
**  ----- BEGIN LICENSE BLOCK -----
**	LIBEP: Enhanced Portability Library (Reduced Edition)
**
**	Copyright (c) 2008-2015, Eric P. Allman.  All rights reserved.
**	Copyright (c) 2015, Regents of the University of California.
**	All rights reserved.
**
**	Permission is hereby granted, without written agreement and without
**	license or royalty fees, to use, copy, modify, and distribute this
**	software and its documentation for any purpose, provided that the above
**	copyright notice and the following two paragraphs appear in all copies
**	of this software.
**
**	IN NO EVENT SHALL REGENTS BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
**	SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST
**	PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION,
**	EVEN IF REGENTS HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
**	REGENTS SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT
**	LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
**	FOR A PARTICULAR PURPOSE. THE SOFTWARE AND ACCOMPANYING DOCUMENTATION,
**	IF ANY, PROVIDED HEREUNDER IS PROVIDED "AS IS". REGENTS HAS NO
**	OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS,
**	OR MODIFICATIONS.
**  ----- END LICENSE BLOCK -----
***********************************************************************/

// generic status codes
#define EP_STAT_OK		EP_STAT_NEW(EP_STAT_SEV_OK, 0, 0, 0)
#define EP_STAT_WARN		_EP_STAT_INTERNAL(WARN, EP_STAT_MOD_GENERIC, 0)
#define EP_STAT_ERROR		_EP_STAT_INTERNAL(ERROR, EP_STAT_MOD_GENERIC, 0)
#define EP_STAT_SEVERE		_EP_STAT_INTERNAL(SEVERE, EP_STAT_MOD_GENERIC, 0)
#define EP_STAT_ABORT		_EP_STAT_INTERNAL(ABORT, EP_STAT_MOD_GENERIC, 0)

// common shared errors
#define EP_STAT_OUT_OF_MEMORY	_EP_STAT_INTERNAL(SEVERE, EP_STAT_MOD_GENERIC, 1)
#define EP_STAT_ARG_OUT_OF_RANGE _EP_STAT_INTERNAL(ERROR, EP_STAT_MOD_GENERIC, 2)
#define EP_STAT_END_OF_FILE	_EP_STAT_INTERNAL(WARN, EP_STAT_MOD_GENERIC, 3)
#define EP_STAT_TIME_BADFORMAT	_EP_STAT_INTERNAL(ERROR, EP_STAT_MOD_GENERIC, 4)
#define EP_STAT_BUF_OVERFLOW	_EP_STAT_INTERNAL(ERROR, EP_STAT_MOD_GENERIC, 5)

// cryptographic module
#define EP_STAT_CRYPTO_DIGEST	_EP_STAT_INTERNAL(ERROR, EP_STAT_MOD_CRYPTO, 1)
#define EP_STAT_CRYPTO_SIGN	_EP_STAT_INTERNAL(ERROR, EP_STAT_MOD_CRYPTO, 2)
#define EP_STAT_CRYPTO_VRFY	_EP_STAT_INTERNAL(ERROR, EP_STAT_MOD_CRYPTO, 3)
#define EP_STAT_CRYPTO_BADSIG	_EP_STAT_INTERNAL(ERROR, EP_STAT_MOD_CRYPTO, 4)
#define EP_STAT_CRYPTO_KEYTYPE	_EP_STAT_INTERNAL(ERROR, EP_STAT_MOD_CRYPTO, 5)
#define EP_STAT_CRYPTO_KEYFORM	_EP_STAT_INTERNAL(ERROR, EP_STAT_MOD_CRYPTO, 6)
#define EP_STAT_CRYPTO_CONVERT	_EP_STAT_INTERNAL(ERROR, EP_STAT_MOD_CRYPTO, 7)
#define EP_STAT_CRYPTO_KEYCREATE _EP_STAT_INTERNAL(ERROR, EP_STAT_MOD_CRYPTO, 8)
#define EP_STAT_CRYPTO_KEYCOMPAT _EP_STAT_INTERNAL(ERROR, EP_STAT_MOD_CRYPTO, 9)
#define EP_STAT_CRYPTO_CIPHER	_EP_STAT_INTERNAL(ERROR, EP_STAT_MOD_CRYPTO, 10)
