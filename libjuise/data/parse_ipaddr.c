/*
 * $Id$
 *
 * General IP address parser.
 *
 * Copyright (c) 1999, 2001-2007, 2011, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <limits.h>

#include "config.h"

#include <libjuise/io/logging.h>
#include <libjuise/data/parse_ip.h>
#include <libjuise/string/strextra.h>

#define SNPRINTF(fmt...) do { if (msg) snprintf(msg, msgsize, fmt); } while (0)

static boolean is_ip_addr;

/******************************************************************************
 * only_ipv4_chars
 *
 * 333.juniper.net may be a valid hostname.  This simple heuristic allows us
 * to return PARSE_ERR when something is obviously not an IP address instead
 * of PARSE_ERR_RESTRICT.
 *****************************************************************************/
static inline boolean
only_ipv4_chars (const char *str)
{
    const char *cp;

    if (!str)
	return TRUE;

    for (cp = str; *cp; cp++)
	if (!(isdigit((int) *cp) || (*cp == '.')))
	    return FALSE;
    return TRUE;
}

#define PARSE_IPV4_RETURN_CODE(_remaining) \
   is_ip_addr ? PARSE_ERR_RESTRICT :       \
   only_ipv4_chars(_remaining) ? PARSE_ERR_RESTRICT : PARSE_ERR

/******************************************************************************
 * parse_nbytes
 *
 * Utility function for parsing ipv4 address or netmask.  inet_pton would be
 * used, but it does not handle these situations:
 *     - not fully specified (255.255 -> 255.255.0.0)
 *     - hexidecimal (0xFFFF00000)
 *     - OSPF area ID as a single decimal integer
 * The bytes returned are in network byte order.
 *
 * Returns:
 *   PARSE_OK(0)            on success
 *   PARSE_ERR(-1)          couldn't grok the str, likely not an addr  
 *   PARSE_ERR_RESTRICT(-2) it's an addr, but failed some restriction
 *****************************************************************************/
static int
parse_nbytes (const char *str, unsigned ipflags, unsigned char *bytes,
	      size_t nbytes, boolean mask, char *msg, size_t msgsize)
{
    const char *cp;
    char *ep;
    size_t i;
    unsigned long num = 0;
    const char *spec = mask ? "ip netmask" : "ip address";

    INSIST_ERR(str != NULL);
    INSIST_ERR(bytes != NULL);

    if (*str == 0) {
	SNPRINTF("%s is not specified", spec);
	errno = EINVAL;
	return PARSE_ERR;
    }
    bzero(bytes, nbytes);
    cp = str;
    if (cp[ 0 ] == '0' && cp[ 1 ] == 'x') {
	num = strtoul(cp, &ep, 16);
	if (cp == ep || (num == ULONG_MAX && strcasecmp(cp, "0xFFFFFFFF"))) {
	    SNPRINTF("invalid hexadecimal %s: '%s'", spec, str);
	    errno = EINVAL;
	    return PARSE_ERR;
	}
	for (i = nbytes; i > 0 && num;) {
	    bytes[ --i ] = num;
	    num >>= NBBY;
	}
    } else {
	for (i = 0; i < nbytes;) {
	    if (*cp == '-')
		ep = const_drop(cp); /* strtoul does not error on neg values */
	    else
		num = strtoul(cp, &ep, 10);
	    if (cp == ep || (num == ULONG_MAX && strcmp(cp, "4294967295"))) {
		SNPRINTF("invalid input at '%s' in %s: '%s'", cp, spec, str);
		errno = EINVAL;
		return PARSE_ERR;
	    }
	    if (i == 0 && (ipflags & PIF_AREAID) && *ep == 0) {
		for (i = nbytes; i > 0 && num;) {
		    bytes[ --i ] = num;
		    num >>= NBBY;
		}
		break;
	    }

	    if (num > IP_ADDR_BYTE_MAX) {
		SNPRINTF("invalid value '%lu' in %s: '%s'", num, spec, str);
		errno = EINVAL;
		return PARSE_IPV4_RETURN_CODE(ep);
	    }

	    bytes[ i++ ] = num;

	    if (*ep == '.') {
		if (*(ep + 1) == '\0') {
		    if (i < nbytes)
			ep++; /* ok to skip this trailing '.' */
		    break; /* end of input, get out */
		} else {
		    cp = ep + 1; /* point to next object to parse */
		}
	    } else {
		break; /* something bad in the input, bail */
	    }
	}
    }
    if (*ep) {
	SNPRINTF("invalid input at '%s' in %s '%s'", ep, spec, str);
	errno = EINVAL;
        if (*ep == IP_DELIMITER_PREFIX) 
            return PARSE_ERR_RESTRICT;
        else
            return PARSE_ERR;
    }
    if ((ipflags & PIF_FULL) && i != nbytes) {
	SNPRINTF("missing information in %s: '%s'", spec, str);
	errno = EINVAL;
	return PARSE_ERR_RESTRICT;
    }
    return 0;
}

/*****************************************************************************/
static int
parse_ipv4addr (const char *str, unsigned long ipflags,
		void *address, int *pfxseen, size_t *pfxlen,
		int *maskseen, void *mask, char *msg, size_t msgsize)
{
    char local[IP_ADDR_BUFLEN], localm[IP_ADDR_BUFLEN];
    char *ep;
    u_int32_t addr, msk;
    const char *input = NULL;
    char *cp = NULL;
    int status;

    INSIST_ERR(str != NULL);
    INSIST_ERR(address != NULL);
    INSIST_ERR(pfxseen != NULL);
    INSIST_ERR(pfxlen != NULL);
    INSIST_ERR(maskseen != NULL);
    INSIST_ERR(mask != NULL);

    *pfxseen = FALSE;
    *maskseen = FALSE;
    *pfxlen = IN_HOST_PLEN; /* always provide default (must on success) */

    /*
     * Not enough evidence that this is an IP address
     */
    is_ip_addr = FALSE;

    /*
     * Handle special designations "all" or "default"
     */
    if ((ipflags & PIF_DEFAULT) &&
	(!strcmp(IP_DESIGNATION_DEFAULT, str) ||
	 !strcmp(IP_DESIGNATION_ALL, str))) {
	/*
	 * Treat as if 0.0.0.0/0 was specifed in 'str'.  Any PIF_* requirements
	 * specified are met by this result.
	 */
	if (address)
	    bzero(address, sizeof(struct in_addr));
	*pfxlen = 0;
	*pfxseen = TRUE;
	return PARSE_OK;
    }

    /*
     * Handle special designations any-unicast/any-multicast
     */
    if ((ipflags & PIF_ANYCAST) && mask && address) {
	struct in_addr *maskp = (struct in_addr *) mask;

	bzero(address, sizeof(struct in_addr));
	bzero(mask, sizeof(struct in_addr));

	if (streq(IP_DESIGNATION_ANY_UNICAST, str)) {
	    maskp->s_addr = IPV4_MASK_ANY_UNICAST;
	    *maskseen = TRUE;
	    return PARSE_OK;
	} else if (streq(IP_DESIGNATION_ANY_MULTICAST, str)) {
	    maskp->s_addr = IPV4_MASK_ANY_UNICAST;
	    *maskseen = TRUE;
	    return PARSE_OK;
	}
    }

    /*
     * Seperate address and prefix length (or mask) if such input is allowed.
     * Only copy the string if we need to seperate.
     */
    if (ipflags & PIF_LEN) {
	cp = strchr(str, IP_DELIMITER_PREFIX);
	if (cp) {
	    if (strlcpy(local, str, sizeof(local)) >= sizeof(local)) {
		SNPRINTF("too long to be a valid ip address: '%s'", str);
		errno = EINVAL;
		return PARSE_ERR;
	    }
	    cp = local + (cp - str);
	    input = local;
	    *cp++ = 0;
	}
    } 
    if (!input)
	input = str;

    /*
     * Parse the address.
     */
    status = parse_nbytes(input, ipflags, address, sizeof(struct in_addr),
                          FALSE, msg, msgsize);
    if (status) 
        return status;

    /*
     * Enough evidence that this is an IP address
     */
    is_ip_addr = TRUE;

    /*
     * If there is a suffix (prefix length or mask), parse it.
     */
    if (cp) {
	if (!strchr(cp, '.')) { /* mask must have at least one period */
	    if (*cp == '-')
		ep = const_drop(cp); /* strtoul does not error on neg values */
	    else
		*pfxlen = strtoul(cp, &ep, 0);
	    if (cp == ep) {
		SNPRINTF("missing or invalid prefix length %s%s%sin address "
			 "'%s'", *ep ? "'" : "", ep, *ep ? "' " : "", str);
		errno = EINVAL;
		return PARSE_ERR_RESTRICT;
	    }
	    if (*ep != 0) {
		SNPRINTF("invalid input at '%s' in address '%s'", ep, str);
		errno = EINVAL;
		return PARSE_ERR_RESTRICT;
	    }
	    if (*pfxlen > IN_HOST_PLEN) {
		SNPRINTF("prefix length '%lu' is larger than %u in address "
			 "'%s'", (u_long) *pfxlen, IN_HOST_PLEN, str);
		errno = EINVAL;
		return PARSE_ERR_RESTRICT;
	    }
	    *pfxseen = TRUE;
	} else if (ipflags & PIF_MASK) {
	    status = parse_nbytes(cp, ipflags, mask, sizeof(struct in_addr),
                                  TRUE, msg, msgsize);
            if (status)
                return status;
	    *maskseen = TRUE;
	} else {
            /*
             * User supplied an addr in mask format, but the token
             * didn't specify a mask.
             */
	    SNPRINTF("invalid input at '%c%s' in address '%s'",
		     IP_DELIMITER_PREFIX, cp, str);
	    errno = EINVAL;
	    return PARSE_ERR_RESTRICT;
	}
    }

    /*
     * Report failure to meet the prefix/mask requirement.
     */
    if ((ipflags & PIF_LENREQ) && !*pfxseen && !*maskseen) {
	SNPRINTF("missing required prefix length %sin address '%s'",
		 (ipflags & PIF_MASK) ? "or mask " : "", str);
	errno = EINVAL;
	return PARSE_ERR_RESTRICT;
    }

    /*
     * For prefix length scenario, PIF_PREFIXONLY indicates zero host portion.
     * For mask scenario, PIF_PREFIXONLY indicates all zero bits of mask must
     * be zero in address.  Thus, user can specify w.x.y.z/30 or
     * w.x.y.z%0xFFFFFFFC and this flag will have the same result.  If prefix
     * length and mask is not specified, this requirement is satisfied.
     */
    if ((ipflags & PIF_PREFIXONLY) && (*pfxseen || *maskseen)) {
	if (*pfxseen && *pfxlen < IN_HOST_PLEN) {
	    addr = *((u_int32_t *)address);
	    msk  = htonl(~((1<<(IN_HOST_PLEN - *pfxlen)) - 1));
	    if (addr & ~msk) {
		addr = addr & msk;
		inet_ntop(AF_INET, &addr, local, sizeof(local));
		SNPRINTF("host portion is not zero (%s%c%lu)",
			 local, IP_DELIMITER_PREFIX, (u_long) *pfxlen);
		errno = EINVAL;
		return PARSE_ERR_RESTRICT;
	    }
	} else if (*maskseen) {
	    addr = *((u_int32_t *)address);
	    msk  = *((u_int32_t *)mask);
	    if (addr & ~msk) {
		addr = addr & msk;
		inet_ntop(AF_INET, &addr, local, sizeof(local));
		inet_ntop(AF_INET, &msk, localm, sizeof(localm));
		SNPRINTF("masked bits are not zero (%s%c%s)",
			 local, IP_DELIMITER_PREFIX, localm);
		errno = EINVAL;
		return PARSE_ERR_RESTRICT;
	    }
	}
    }
    return PARSE_OK;
}

/*****************************************************************************/
static int
parse_ipv6addr (const char *str, unsigned long ipflags,
		void *address, int *pfxseen, size_t *pfxlen,
		int *maskseen, void *mask, char *msg, size_t msgsize)
{
    char local[IPV6_ADDR_BUFLEN];
    char *ep;
    int status;
    const char *input = NULL;
    char *cp = NULL;

    INSIST_ERR(str != NULL);
    INSIST_ERR(address != NULL);
    INSIST_ERR(pfxseen != NULL);
    INSIST_ERR(pfxlen != NULL);
    INSIST_ERR(maskseen != NULL);
    INSIST_ERR(mask != NULL);

    *pfxseen = FALSE;
    *maskseen = FALSE;
    *pfxlen = IN6_HOST_PLEN; /* always provide default (must on success) */

    if (*str == 0) {
	SNPRINTF("ip address is not specified");
	errno = EINVAL;
	return PARSE_ERR;
    }

    /*
     * Handle special designations "all" or "default"
     */
    if ((ipflags & PIF_DEFAULT) &&
	(!strcmp(IP_DESIGNATION_DEFAULT, str) ||
	 !strcmp(IP_DESIGNATION_ALL, str))) {
	/*
	 * Treat as if ::/0 was specifed in 'str'.  Any PIF_* requirements
	 * specified are met by this result.
	 */
	if (address)
	    bzero(address, sizeof(struct in6_addr));
	*pfxlen = 0;
	*pfxseen = TRUE;
	return PARSE_OK;
    }

    /*
     * Seperate address and prefix length (or mask) if such input is allowed.
     * Only copy the string if we need to seperate.
     */
    if (ipflags & PIF_LEN) {
	cp = strchr(str, IP_DELIMITER_PREFIX);
	if (cp) {
	    if (strlcpy(local, str, sizeof(local)) >= sizeof(local)) {
		SNPRINTF("too long to be a valid ipv6 address: '%s'", str);
		errno = EINVAL;
		return PARSE_ERR;
	    }
	    cp = local + (cp - str);
	    input = local;
	    *cp++ = 0;
	}
    }
    if (!input)
	input = str;

    /*
     * Parse the address.
     */
    status = inet_pton(AF_INET6, input, address);
    if (status != 1) {
	SNPRINTF("'%s' is not a valid ipv6 address%s%s", input,
		 status < 0 ? ": " : "", status < 0 ? strerror(errno) : "");
	return PARSE_ERR;
    }

    /*
     * If there is a suffix (prefix length or mask), parse it.
     */
    if (cp) {
	if (!strchr(cp, ':')) { /* mask must have at least one colon */
	    if (*cp == '-')
		ep = const_drop(cp); /* strtoul does not error on neg values */
	    else
		*pfxlen = strtoul(cp, &ep, 10);
	    if (cp == ep) {
		SNPRINTF("missing or invalid prefix length %s%s%sin address "
			 "'%s'", *ep ? "'" : "", ep, *ep ? "' " : "", str);
		errno = EINVAL;
		return PARSE_ERR_RESTRICT;
	    }
	    if (*ep != 0) {
		SNPRINTF("invalid input at '%s' in address '%s'", ep, str);
		errno = EINVAL;
		return PARSE_ERR_RESTRICT;
	    }
	    if (*pfxlen > IN6_HOST_PLEN) {
		SNPRINTF("prefix length '%lu' is larger than %u in address "
			 "'%s'", (u_long) *pfxlen, IN6_HOST_PLEN, str);
		errno = EINVAL;
		return PARSE_ERR_RESTRICT;
	    }
	    *pfxseen = TRUE;
	} else if (ipflags & PIF_MASK) {
	    status = inet_pton(AF_INET6, cp, mask);
	    if (status != 1) {
		SNPRINTF("'%s' is not a valid ipv6 mask%s%s", cp,
			 status < 0 ? ": " : "",
			 status < 0 ? strerror(errno) : "");
		return PARSE_ERR_RESTRICT;
	    }
	    *maskseen = TRUE;
	} else {
	    SNPRINTF("invalid input at '%c%s' in address '%s'",
		     IP_DELIMITER_PREFIX, cp, str);
	    errno = EINVAL;
	    return PARSE_ERR_RESTRICT;
	}
    }

    /*
     * Report failure to meet the prefix/mask requirement.
     */
    if ((ipflags & PIF_LENREQ) && !*pfxseen && !*maskseen) {
	SNPRINTF("missing required prefix length %sin address '%s'",
		 (ipflags & PIF_MASK) ? "or mask " : "", str);
	errno = EINVAL;
	return PARSE_ERR_RESTRICT;
    }

    /*
     * For prefix length scenario, PIF_PREFIXONLY indicates zero host portion.
     * For mask scenario, PIF_PREFIXONLY indicates all zero bits of mask must
     * be zero in address.  Thus, user can specify ::/16 or ::%FFFF:: and
     * this flag will have the same result.  If prefix length and mask is not
     * specified, this requirement is satisfied.
     */
    if ((ipflags & PIF_PREFIXONLY) && (*pfxseen || *maskseen)) {
	u_char byte, last_bits;
	struct in6_addr *addr = address;

	if (*pfxseen && *pfxlen < IN6_HOST_PLEN) {
	    /*
	     * Compute the number of the byte that contains the last
	     * network bits (and may contain host bits).  If the prefix
	     * length is 0, this will compute to -1, and so the byte-checking
	     * loop below will start checking bytes at byte 0.
	     */
	    byte = ((*pfxlen + NBBY - 1) / NBBY) - 1;

	    /*
	     * If that last network byte also contains host bits, check
	     * them first
	     */
	    if (*pfxlen % NBBY) { /* byte will by > 0 here */
		last_bits = 0xff >> (*pfxlen % NBBY);
		if ((addr->s6_addr[byte] & last_bits) != 0) {
		    SNPRINTF("host portion of '%s' must be zero", str);
		    errno = EINVAL;
		    return PARSE_ERR_RESTRICT;
		}
	    }

	    /*
	     * Check the rest of the bytes (no bits should be set).
	     */
	    for (byte = byte + 1; byte < sizeof(*addr); byte++) {
		if (addr->s6_addr[byte] != 0) {
		    SNPRINTF("host portion of '%s' must be zero", str);
		    errno = EINVAL;
		    return PARSE_ERR_RESTRICT;
		}
	    }
	} else if (*maskseen) {
	    struct in6_addr *msk = mask;
	    for (byte = 0; byte < sizeof(*msk); byte++) {
		if (addr->s6_addr[byte] & ~msk->s6_addr[byte]) {
		    SNPRINTF("masked bits of '%s' must be zero", str);
		    errno = EINVAL;
		    return PARSE_ERR_RESTRICT;
		}
	    }
	}
    }
    return PARSE_OK;
}

/*****************************************************************************
 * parse_ipaddr_is_martian
 *    parse the ipaddress to be one of the well-known martian addresses
 *
 * Parameters:
 *   'af'           - address family
 *   'address'      - address in network byte order
 * 
 * Returns:
 *   TRUE(1)            if martian address 
 *   FALSE(0)           if not martian addrses 
 ****************************************************************************/
static boolean
parse_ipaddr_is_martian (int af, void *address)
{
    if (af == AF_INET) {
	    u_int32_t addr = ntohl(*((u_int32_t *)address));
        /*   
         *  The following addresses are well known martians:
         *    - 0.0.0.0
         *    - 127.0.0.0/8       (loopback)
         *    - 128.0.0.0/16      (martian)
         *    - 191.255.0.0/16    (martian)
         *    - 192.0.0.0/24      (martian)
         *    - 223.255.255.0/24  (martian)
         *    - 224.0.0.0/4       (multicast)
         *    - 240.0.0.0/4       (reserved)
         *    - 255.255.255.255   (broadcast)
         */
        return ((addr == 0x00000000) ||
                ((addr & 0xff000000) == 0x7f000000) ||
                ((addr & 0xffff0000) == 0x80000000) ||
                ((addr & 0xffff0000) == 0xbfff0000) ||
                ((addr & 0xffffff00) == 0xc0000000) ||
                ((addr & 0xffffff00) == 0xdfffff00) ||
                ((addr & 0xf0000000) == 0xe0000000) ||
                ((addr & 0xf0000000) == 0xf0000000) ||
                (addr == 0xffffffff));
    } else if (af == AF_INET6) {
        /* TBD: martian checks for ipv6 addresses */
        return FALSE;
    } else {
          INSIST_ERR("Can only handle AF_INET or AF_INET6"); 
    }
    return FALSE;
}

/*****************************************************************************
 * parse_ipaddr_is_unicast
 *    parse the ipaddress to be one of the well-known unicast addresses.
 *
 * Parameters:
 *   'af'           - address family
 *   'address'      - address in network byte order
 *
 * Returns:
 *   TRUE(1)            if unicast address
 *   FALSE(0)           if not unicast addrses
 ****************************************************************************/
boolean
parse_ipaddr_is_unicast (int af, void *address)
{
    if (af == AF_INET) {
	u_int32_t addr = ntohl(*((u_int32_t *) address));

	if ((addr == 0x00000000) ||
	    (addr == 0xffffffff) ||
	    IN_MULTICAST(addr)) {
	    return FALSE;
	}
    } else if (af == AF_INET6) {
	struct in6_addr *addr = address;

	if (IN6_IS_ADDR_MULTICAST(addr) ||
	    IN6_IS_ADDR_UNSPECIFIED(addr)) {
	    return FALSE;
	}
    } else {
	return FALSE;
    }

    return TRUE;
}

/*****************************************************************************
 * ipv6_address_type
 *    Returns the type of IPv6 address i.e. link-local, global, multicast
 *
 * Parameters:
 *   'address'      - address in network byte order
 *
 * Returns:
 *   type of IPv6 address
 ****************************************************************************/
ipv6addr_type_t
ipv6_address_type(void *address)
{
    struct in6_addr *addr = address;

    if (NULL == addr) {
	return ADDR_UNSPECIFIED;
    }

    if (IN6_IS_ADDR_UNSPECIFIED(addr)) {
	return ADDR_UNSPECIFIED;
    } else if (IN6_IS_ADDR_LINKLOCAL(addr)) {
	return ADDR_LINK_LOCAL;
    } else if (IN6_IS_ADDR_SITELOCAL(addr)) {
	return ADDR_SITE_LOCAL;
    } else if (IN6_IS_ADDR_MULTICAST(addr)) {
	return ADDR_MULTICAST;
    } else {
	return ADDR_GLOBAL;
    }
}
/*****************************************************************************
 * parse_ipaddr
 *    parse the various forms of ip addresses directed by 'ipflags' (PIF_*)
 *
 * Parameters:
 *   'af'           - address family, set if not specified
 *   'str'          - address to be parsed
 *   'ipflags'      - directs parsing of 'str', see PIF_ flags
 *   'address'      - result address in network byte order
 *   'address_size' - size of 'address' in bytes
 *   'pfxseen'      - prefix length (/'pfxlen') was seen in 'str'
 *   'pfxlen'       - result prefix length, defaults to max value (on success)
 *   'maskseen'     - mask (%'mask') was seen in 'str'
 *   'mask'         - result mask in network byte order
 *   'mask_size'    - size of 'mask' in bytes
 *   'msg'          - error message when error is returned
 *   'msgsize'      - size of 'msg' buffer
 * 
 * Returns:
 *   PARSE_OK(0)            on success
 *   PARSE_ERR(-1)          couldn't grok the str, likely not an addr  
 *   PARSE_ERR_RESTRICT(-2) it's an addr, but failed some restriction  
 ****************************************************************************/
parse_retcode_t
parse_ipaddr (int *af, const char *str, unsigned long ipflags,
	      void *address, size_t address_size, int *pfxseen, size_t *pfxlen,
	      int *maskseen, void *mask, size_t mask_size,
	      char *msg, size_t msgsize)
{
    int pfxseen_l;
    size_t pfxlen_l;
    int maskseen_l;
    int status;
    struct in_addr addr_l;
    struct in_addr mask_l;
    struct in6_addr addr6_l;
    struct in6_addr mask6_l;

    INSIST_ERR(af != NULL);
    INSIST_ERR(str != NULL);

    /*
     * Most arguments are optional.  Supply memory for those that are pointers.
     */
    if (!pfxseen)
	pfxseen = &pfxseen_l;
    if (!pfxlen)
	pfxlen = &pfxlen_l;
    if (!maskseen)
	maskseen = &maskseen_l;

    /*
     * If 'af' is specifically unspecified, set the address family now.
     * IPv6 addresses must have a ':'.  IPv4 address must NOT have a ':'.
     * "all" and "default" will be assumed IPv4 if AF_UNSPEC.
     */
    if (*af == AF_UNSPEC)
	*af = strchr(str, ':') ? AF_INET6 : AF_INET;

    /*
     * Parse 'str' based on address type.
     */
    switch (*af) {
    case AF_INET:
	if (!address) {
	    address = &addr_l;
	} else if (address_size < sizeof(struct in_addr)) {
	    SNPRINTF("'address' not large enough to hold result");
	    errno = ENOSPC;
	    return PARSE_ERR_RESTRICT;
	}
	if (!mask) {
	    mask = &mask_l;
	} else if (mask_size < sizeof(struct in_addr)) {
	    SNPRINTF("'mask' not large enough to hold result");
	    errno = ENOSPC;
	    return PARSE_ERR_RESTRICT;
	}
	status = parse_ipv4addr(str, ipflags, address, pfxseen, pfxlen,
                                maskseen, mask, msg, msgsize);
        if (status) 
            return status;

        if ((ipflags & PIF_MULTICAST_ONLY) &&
            !IN_MULTICAST(ntohl(*((u_int32_t *)address)))) {
            
            SNPRINTF("invalid multicast address - '%s'",  str);
            return PARSE_ERR_RESTRICT;
        }

        if ((ipflags & PIF_MARTIAN) &&
            parse_ipaddr_is_martian(*af, address)) {
            SNPRINTF("martian address not allowed - '%s'", str);
            return PARSE_ERR_RESTRICT;
        }

	if ((ipflags & PIF_UNICASTONLY) &&
	    !parse_ipaddr_is_unicast(*af, address)) {
            SNPRINTF("invalid unicast address - '%s'", str);
            return PARSE_ERR_RESTRICT;
	}

        return PARSE_OK;
	break;
    case AF_INET6:
	if (!address) {
	    address = &addr6_l;
	} else if (address_size < sizeof(struct in6_addr)) {
	    SNPRINTF("'address' not large enough to hold result");
	    errno = ENOSPC;
	    return PARSE_ERR_RESTRICT;
	}
	if (!mask) {
	    mask = &mask6_l;
	} else if (mask_size < sizeof(struct in6_addr)) {
	    SNPRINTF("'mask' not large enough to hold result");
	    errno = ENOSPC;
	    return PARSE_ERR_RESTRICT;
	}
	status = parse_ipv6addr(str, ipflags, address, pfxseen, pfxlen,
                                maskseen, mask, msg, msgsize);
        if (status)
            return status;

        if ((ipflags & PIF_MULTICAST_ONLY) &&
            !IN6_IS_ADDR_MULTICAST((struct in6_addr *) address)) {

            SNPRINTF("invalid multicast address - '%s'",  str);
            return PARSE_ERR_RESTRICT;
        } 

        if ((ipflags & PIF_MARTIAN) &&
            parse_ipaddr_is_martian(*af, address)) {
            SNPRINTF("martian address not allowed - '%s'", str);
            return PARSE_ERR_RESTRICT;
        }

	if ((ipflags & PIF_UNICASTONLY) &&
	    !parse_ipaddr_is_unicast(*af, address)) {
            SNPRINTF("invalid unicast address - '%s'", str);
            return PARSE_ERR_RESTRICT;
	}

        return PARSE_OK;
	break;
    }
    SNPRINTF("unsupported address family");
    errno = EAFNOSUPPORT;
    return PARSE_ERR;
}


#if defined(UNIT_TEST)

const char *v4addrs[] = {
    "all",
    "default",
    "4278255360", /* FF00FF00 OSPF AREAID */
    "4294967295", /* FFFFFFFF OSPF AREAID */
    "3.4.5.6",
    "023.024.025.026",
    "3.4.5",
    "3.4",
    "3",
    "3.",
    "3.4.5.6.7",
    "3.4.5.6.",
    "3.4.5.6  ",
    "255.255.256.255",
    "FF.FF.FF.FF",
    "255.255.FF.FF",
    "255.255.-1.FF",
    "255.255.-4294967294.FF",
    "255.255.444444444444444294967294.FF",
    "255.255.5-5.255",
    "0xFFFFFFFF",
    "0xFffFFFfF",
    "0xAABBCCDD",
    "0xaabbccdd",
    "-0xAABBCCDD",
    "0xAABB-CCDD",
    "0x-AABBCCDD",
    "3.4/16",
    "0.0.3.3/16",
    "3.4.0.0/16",
    "3.4.5.6/24",
    "3.4.5.6/-4294967294",
    "3.4.5.6/08",
    "3.4.5.6/09",
    "3.4.5.6/024",
    "3.4.5.6/0x24",
    "3.4.5.6/255",
    "3.4.5.6/255.",
    "3.4.5.6/255.255.255.",
    "3.4.5.6/255.255.255.0",
    "3.4.5.6/24",
    "3.4.5.6/24bb",
    "3.4.5.6/24.25",
    "3.0.5.0/24.0.25.0",
    "24.0.25.0/24.0.25.0",
    "3.0.5.0/255.0.255.0",
    "3.4.5.6/24.25.26.27",
    "3.4.5.6/24.25.26.27.28",
    "3.4.5.6/24.25.26.27.",
    "3.4.5.6/24.25.26.27 ",
    "3.4.5.6 /24.25.26.27",
    "3.4.5.6/-24.25.26.27",
    "333.juniper.net",
    "333",
    "1.333",
    "1.256.a",
    "1.a.256",
    "1.2.3.4.5",
    "1.2.3.4.555",
    "0.0.0.0",
    "127.0.0.0",
    "128.0.0.1",
    "191.255.0.0",
    "192.0.0.3",
    "223.255.255.4",
    "224.10.0.0",
    "240.0.10.0",
    "255.255.255.255",
    "",
    NULL
};

const char *v6addrs[] = {
    "all",
    "default",
    "::4.5.6.7",
    "3:4::/24",
    "3:4::/32",
    "3:4::5/32",
    "FFFF:FFFF::/24",
    "FFFF:FFFF::/25",
    "12AB:0:0:CD30::",
    "FFFF:FFFF::/FFFF:FFFF::",
    "FFFF:FFFF::/FFFF:FFFE::",
    "FFFF:FFF0::/FFFF:FFF0::",
    "FFFF:FFC0::/FFFF:FFC0::",
    "FFFF:FFC0::/FFFF:FFB0::",
    "FFFF:FF80::/FFFF:FF80::",
    "FFFF:FF40::/FFFF:FF80::",
    "3::4",
    "3::4/",
    "3::4/3",
    "3::4/3::4",
    "3:4::5/32bb",
    "3::4/-4294967294",
    "3::4/08",
    "3::4/009",
    "3::4/024",
    "3::4/0x24",
    "::",
    "3::41::7.5/",
    "3::4/1::7.5",
    "3000::0/::0.255.255.255",
    "0:0:4:5::6:7",
    "1:2:3:4:5:6:7:0",
    "1:2:3:4:5:6:0:0",
    "0:0:4:0:0:0:7:0",
    "0:0:0:0:0:0:4.5.6.7",
    "0:0:0:0:0:0:4.5.6.7zz",
    "0:0:0:0:0:0:44:44",
    "::4.4.4.4",
    "1::4.5.6.7",
    "::FFFF:15.15.15.15",
    "1::FFFF:15.15.15.15",
    "1::15.15.15.15",
    "1::255.255.255.255",
    "1::FF.FF.FF.FF",
    "FF:FF:FF:FF:FF:FF:FF:FF",
    "1::7.5",
    "3:4:5",
    "3:4:5:6",
    "3:0:0:4:0:0:6:0",
    "3::4",
    ":3::4",
    "3::4:",
    ":3::4:",
    "0:0:0:0:0:0:0.0.0.0",
    "0:0:0:0:0:0:172.0.0.0",
    "3:0:0:0:0:4:172.0.0.0",
    "1::3:172.16.22.14",
    "0:0:0:0:0:0:0:0",
    "0:0:0:0:0:0:ABCD:EF00",
    "",
    NULL
};

const unsigned flag_combinations[] = {
    0,
    PIF_LEN,
    PIF_LEN | PIF_LENREQ | PIF_DEFAULT,
    PIF_AREAID,
    PIF_LEN | PIF_MASK,
    PIF_LEN | PIF_MASK | PIF_LENREQ,
    PIF_LEN | PIF_MASK | PIF_FULL,
    PIF_LEN | PIF_MASK | PIF_PREFIXONLY,
    PIF_LEN | PIF_MASK | PIF_MARTIAN, 
    0xFFFFFFFF
};

static const char *
dashes (size_t num)
{
    static char buf[256];
    num = (num > sizeof(buf) - 1) ? sizeof(buf) - 1 : num;
    memset(buf, '-', num);
    buf[num] = '\0';
    return buf;
}

static void
print_flags (unsigned ipflags)
{
    if (ipflags & PIF_LEN)
	printf("PIF_LEN ");
    if (ipflags & PIF_LENREQ)
	printf("PIF_LENREQ ");
    if (ipflags & PIF_FULL)
	printf("PIF_FULL ");
    if (ipflags & PIF_MASK)
	printf("PIF_MASK ");
    if (ipflags & PIF_DEFAULT)
	printf("PIF_DEFAULT ");
    if (ipflags & PIF_AREAID)
	printf("PIF_AREAID ");
    if (ipflags & PIF_LENOPT)
	printf("PIF_LENOPT ");
    if (ipflags & PIF_PREFIXONLY)
	printf("PIF_PREFIXONLY ");
    if (ipflags & PIF_MARTIAN)
	printf("PIF_MARTIAN ");
    printf("\n");
}

static const char *
getafstr (int af)
{
    switch (af) {
    case AF_UNSPEC:
	return "AF_UNSPEC";
	break;
    case AF_INET:
	return "AF_INET";
	break;
    case AF_INET6:
	return "AF_INET6";
	break;
    default:
	return "unknown";
	break;
    }
}

static const char *
err_str (int item)
{
    switch (item) {
    case PARSE_ERR:
	return "PARSE_ERR";
    case PARSE_ERR_RESTRICT:
	return "PARSE_ERR_RESTRICT";
    case PARSE_OK:
	return "PARSE_OK";
    default:
	return "<UNKNOWN>";
    }
    return "<UNKNOWN>";
}

int
main (int argc __unused, char **argv __unused)
{
    struct in_addr ipv4_addr, ipv4_mask;
    struct in6_addr ipv6_addr, ipv6_mask;
    const char **addr, **end;
    const unsigned *ipflags;
    size_t pfxlen;
    int af, pfxseen, maskseen;
    parse_retcode_t rc;
    char buf[256], line[256];

    ipflags = flag_combinations;
    while (*ipflags != 0xFFFFFFFF) {
	print_flags(*ipflags);
	printf("%-40s%*s%s\n", "Input", 5, "", "Output");
	printf("%s%*s%s\n", dashes(40), 5, "", dashes(35));
	/* v4 */
	af = AF_INET;
	addr = v4addrs;
	while (*addr) {
	    printf("%-40s%*s", *addr, 5, "");
	    buf[0] = '\0';
	    rc = parse_ipaddr(&af, *addr, *ipflags,
			      &ipv4_addr, sizeof(ipv4_addr), &pfxseen, &pfxlen,
			      &maskseen, &ipv4_mask, sizeof(ipv4_mask),
			      buf, sizeof(buf));
	    if (rc == PARSE_OK) {
		format_ipaddr(af, &ipv4_addr, FALSE,
			      pfxseen, pfxlen, maskseen, &ipv4_mask,
			      buf, sizeof(buf));
		printf("%s\n", buf);
	    } else {
		printf("%s %s\n", err_str(rc), buf);
	    }
	    addr++;
	}
	/* v6 */
	af = AF_INET6;
	addr = v6addrs;
	while (*addr) {
	    printf("%-40s%*s", *addr, 5, "");
	    buf[0] = '\0';
	    rc = parse_ipaddr(&af, *addr, *ipflags,
			      &ipv6_addr, sizeof(ipv6_addr), &pfxseen, &pfxlen,
			      &maskseen, &ipv6_mask, sizeof(ipv6_mask),
			      buf, sizeof(buf));
	    if (rc == PARSE_OK) {
		format_ipaddr(af, &ipv6_addr, FALSE,
			      pfxseen, pfxlen, maskseen, &ipv6_mask,
			      buf, sizeof(buf));
		printf("%s\n", buf);
	    } else {
		printf("%s %s\n", err_str(rc), buf);
	    }
	    addr++;
	}
	ipflags++;
	printf("\n");
    }
    /* format_ipv6addr_partial */
    printf("format_ipv6addr_partial TEST\n");
    printf("%s%*s%s\n", dashes(40), 5, "", dashes(35));
    addr = v6addrs;
    end = addr + 9;
    af = AF_INET6;
    while (*addr && addr < end) {
	printf("%-40s%*s", *addr, 5, "");
	rc = parse_ipaddr(&af, *addr, PIF_LEN, &ipv6_addr, sizeof(ipv6_addr),
			  &pfxseen, &pfxlen, &maskseen, &ipv6_mask,
			  sizeof(ipv6_mask), buf, sizeof(buf));
	if (rc == PARSE_OK) {
	    format_ipv6addr_partial(&ipv6_addr, pfxlen, pfxseen,
				    buf, sizeof(buf));
	    printf("%s\n", buf);
	} else {
	    printf("%s %s\n", err_str(rc), buf);
	}
	addr++;
    }
    printf("\n");

    af = AF_UNSPEC;
    for (;;) {
	if (fgets(line, sizeof(line), stdin) == NULL) break;
	line[ strlen(line) - 1 ] = 0;
	buf[0] = 0;
	if (!strcmp(line, "4")) {
	    af = AF_INET;
	    continue;
	} else if (!strcmp(line, "6")) {
	    af = AF_INET6;
	    continue;
	} else if (!strcmp(line, "u")) {
	    af = AF_UNSPEC;
	    continue;
	}
	printf("attempt to parse af = %s, '%s'\n", getafstr(af), line);
	rc = parse_ipaddr(&af, line, 
                          PIF_LEN | PIF_MASK | PIF_PREFIXONLY | PIF_MARTIAN,
			  &ipv6_addr, sizeof(ipv6_addr),
			  &pfxseen, &pfxlen, &maskseen, &ipv6_mask,
			  sizeof(ipv6_mask), buf, sizeof(buf));
	if (rc != PARSE_OK) {
	    printf("rc %s: buf '%s'\n", err_str(rc), buf);
	} else {
	    format_ipaddr(af, &ipv6_addr, FALSE,
			  pfxseen, pfxlen, maskseen, &ipv6_mask,
			  buf, sizeof(buf));
	    printf("  '%s' -> '%s' pfxseen %d maskseen %d af %s\n",
		   line, buf, pfxseen, maskseen, getafstr(af));
	}
    }

    return 0;
}

#endif /* UNIT_TEST */

