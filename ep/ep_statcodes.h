/* vim: set ai sw=8 sts=8 ts=8 : */

/***********************************************************************
**  ----- BEGIN LICENSE BLOCK -----
**	LIBEP: Enhanced Portability Library (Reduced Edition)
**
**	Copyright (c) 2008-2015, Eric P. Allman.  All rights reserved.
**	Copyright (c) 2015, Regents of the University of California.
**
**	Redistribution and use in source and binary forms, with or without
**	modification, are permitted provided that the following conditions
**	are met:
**
**	1. Redistributions of source code must retain the above copyright
**	notice, this list of conditions and the following disclaimer.
**
**	2. Redistributions in binary form must reproduce the above copyright
**	notice, this list of conditions and the following disclaimer in the
**	documentation and/or other materials provided with the distribution.
**
**	3. Neither the name of the copyright holder nor the names of its
**	contributors may be used to endorse or promote products derived
**	from this software without specific prior written permission.
**
**	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
**	"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
**	LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
**	FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
**	COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
**	INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
**	BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
**	LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
**	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
**	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
**	ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
**	POSSIBILITY OF SUCH DAMAGE.
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
