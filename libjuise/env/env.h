/*
 * $Id$
 *
 * Copyright (c) 2011, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 */

#ifndef LIBJUISE_ENV_SOCKETS_H
#define LIBJUISE_ENV_SOCKETS_H

#undef strerror_r		/* Strange cygwin-ness */

#ifndef SA_NOCLDWAIT
#define SA_NOCLDWAIT 0
#endif

#ifdef HAVE_PRINTFLIKE		/* Cygwin lacks this */
#define PRINTFLIKE(_a, _b) __printflike(_a, _b)
#else
#define PRINTFLIKE(_a, _b)
#endif

#ifndef FD_COPY			/* Cygwin lacks this */
#define FD_COPY(f, t) (void)(*(t) = *(f))
#endif

#ifdef HAVE_IOCTLSOCKET		/* Cygwin under MS-Windows */
#include <windows.h>
#endif /* HAVE_IOCTLSOCKET */

#endif /* LIBJUISE_ENV_SOCKETS_H */
