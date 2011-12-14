/*
 * $Id$
 *
 * Copyright (c) 1999-2007, 2011, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 */
#ifndef __JNX_PARSE_IP_H__
#define __JNX_PARSE_IP_H__

/**
 * @file parse_ip.h
 * @brief 
 * Generalized IP address parser APIs.
 */

#include <libjuise/common/aux_types.h>
#include <libjuise/data/parse.h>
#include <netinet/in.h>

#ifndef IN_HOST_PLEN
#define IN_HOST_PLEN 32		/* Max prefix length for ipv4 addresses */
#endif

#ifndef IN6_HOST_PLEN
#define IN6_HOST_PLEN 128	/* Max prefix length for ipv6 addresses */
#endif
/*
 * Use C linkage when using a C++ compiler
 */
#ifdef __cplusplus
extern "C" { 
    namespace junos {
#endif /* __cplusplus */

#define IP_DELIMITER_PREFIX	'/'
#define IP_DESIGNATION_ALL	"all"
#define IP_DESIGNATION_DEFAULT	"default"
#define IP_DESIGNATION_ANY_UNICAST	"any-unicast"
#define IP_DESIGNATION_ANY_MULTICAST	"any-multicast"

/*
 * When a special keyword (e.g. 'any-unicast') is specified 
 * the address will be set to 0 and the mask
 * will be set to one of the following (otherwise illegal) mask.
 */
#define IPV4_MASK_ANY_UNICAST    0x00000001
#define IPV4_MASK_ANY_MULTICAST  0x00000002

/**
 * @brief
 * @c IP_ADDR_BUFLEN includes @c NULL terminator and accomodates
 * "255.255.255.255/32" or "255.255.255.255/255.255.255.255"
 */
#define IP_ADDR_BUFLEN		(INET_ADDRSTRLEN * 2) /* see above */
#define IP_ADDR_BYTE_MAX	255
#define IP_ADDR_BYTE_NUM	(sizeof(struct in_addr))

/**
 * @brief
 * IPv6 address buffer length.
 *
 * @c IPV6_ADDR_BUFLEN includes NULL terminator and accomodates any of:
 * "FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF/128" or
 * "FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF/FFFF:FFFF:FFFF:FFFF:FFFF:FFF... or
 * "0000:0000:0000:0000:0000:0000:255.255.255.255/128" or
 * "0000:0000:0000:0000:0000:0000:255.255.255.255/0000:0000:0000:0000:00...
 */
#define IPV6_ADDR_BUFLEN	(INET6_ADDRSTRLEN * 2) /* see above */

/**
 * @brief
 * Flags for parse_ipaddr().
 *
 * PIF_LENOPT indicates that if the prefix length is not specified, then this
 * address is not meant have a prefix associated with it: so DO NOT provide a
 * value for prefix length.  The PIF_LENOPT option has no effect on the
 * operation of parse_ipaddr.  PIF_LENOPT is used by the type conversion
 * methods (convert.c).
 *
 * The type conversion methods use PIF_LEN && !PIF_LENOPT to indicate that
 * the prefix length should always part of the result string (whether the
 * prefix length was specified or not).  The type conversion methods use
 * PIF_LEN && PIF_LENOPT to indicate that the prefix length will be part of
 * the result string only if it was specified to begin with.
 */
#define PIF_LEN		(1<<0)	/**< Allow prefix length (see above) */
#define PIF_LENREQ	(1<<1)	/**< Require prefix length */
#define PIF_FULL	(1<<2)	/**< Require full specification (implied w/IPv6)*/
#define PIF_MASK	(1<<3)	/**< /mask can substitute for /prefix-length */
#define PIF_DEFAULT	(1<<4)	/**< "all" or "default" -> 0/0 */
#define PIF_AREAID	(1<<5)	/**< OSPF area id syntax; grim */
#define PIF_LENOPT	(1<<6)	/**< Do not assume prefix length (see above) */
#define PIF_PREFIXONLY	(1<<7)	/**< Prefix only: require host bits to be zero */
#define PIF_MULTICAST_ONLY (1<<8) /**< Allow Multicast address only */
#define PIF_ANYCAST     (1<<9)  /**< 'any-unicast' */
#define PIF_MARTIAN     (1<<10)  /**< check for martian addresses */
#define PIF_UNICASTONLY (1<<11)  /**< Allow Unicast address only */

#define IN_ADDR_BYTE(_a, _b) (((const unsigned char *)&((_a)->s_addr))[_b])
#define IN6_ADDR_HSHORT(_a, _s) ntohs(_a->__u6_addr.__u6_addr16[_s])

/**
 * @brief
 * Types of IPv6 Address
 */
typedef enum ipv6addr_type_e {
    ADDR_UNSPECIFIED = 0,     /**< Unspecified address */
    ADDR_LINK_LOCAL = 1,      /**< link-local address */
    ADDR_SITE_LOCAL = 2,      /**< link-local address */
    ADDR_MULTICAST = 3,       /**< multicast address */
    ADDR_GLOBAL = 4,          /**< global address */
} ipv6addr_type_t; 

/**
 * @brief
 * Parses the various forms of IP addresses directed by @a ipflags (@c PIF_*).
 *
 * @param[in,out] af
 *     Address family, set if not specified
 * @param[in]     str
 *     Address to be parsed
 * @param[in] ipflags
 *     Directs parsing of @a str (see @c PIF_ flags)
 * @param[out] address
 *     Result address (in network byte order)
 * @param[out] address_size
 *     Size of result address buffer, in bytes
 * @param[out] pfxseen
 *     Prefix length (/'pfxlen') was seen in string
 * @param[out] pfxlen
 *     Result prefix length.  Default is the maximum value.
 * @param[out] maskseen
 *     Mask (%'mask') was seen in string
 * @param[out] mask
 *     Result mask (in network byte order)
 * @param[in] mask_size
 *     Size of of the result mask, in bytes
 * @param[out] msg
 *     When an error occurs, the error message is written here
 * @param[in] msgsize
 *     Size of the message buffer
 *
 * @return 
 *     The status of the parsing.
 */
parse_retcode_t
parse_ipaddr (int *af, const char *str, unsigned long ipflags,
	      void *address, size_t address_size, int *pfxseen, size_t *pfxlen,
	      int *maskseen, void *mask, size_t mask_size,
	      char *msg, size_t msgsize);

/**
 * @brief
 * Produces textual form of address, followed by prefix or netmask.
 *
 * @param[in]  af
 *     Address family
 * @param[in]  address
       Address to be formatted (in network byte order)
 * @param[in]  canonicalize
 *     Indicates padding with zeros (produce sortable key)
 * @param[in]  pfxseen
 *     Attach prefix length /'pfxlen' to the end of the output buffer
 * @param[in]  pfxlen
 *     Prefix length.
 * @param[in]  maskseen
 *     Attach mask %'mask' to the end of the output buffer
 * @param[in]  mask
 *     Pointer to the mask (in network byte order)
 * @param[out] out
 *     Output buffer
 * @param[in]  outsize
 *     Size of the output buffer, in bytes.
 * @par Usage 
 *     @li To return address alone in ASCII format, both @a pfxseen and 
 *     @a maskseen should be set to 0.
 *     @li To return address with its prefix in ASCII format, @a pfxseen
 *     should be set to 1. When @a pfxseen is set to 1, the string in @a out 
 *     will be in the form '@a address/@a pfxlen'. 
 *     @li To return an address with its mask in ASCII format, @a maskseen
 *     should be set to 1. When @a maskseen is set to 1, the string in @a out 
 *     will be in the form '@a address%@a mask'.
 * @return 
 *     0 when not able to format string, error value will be stored in
 *     the global variable @c errno; otherwise the size of the string in
 *     @a out, not including the null-terminator character.
 */
size_t
format_ipaddr (int af, const void *address, boolean canonicalize,
	       int pfxseen, size_t pfxlen, int maskseen, const void *mask, 
	       char *out, size_t outsize);

/**
 * @brief
 * Produces textual form of address with bits masked, followed by prefix
 * length, if required.
 *
 * @param[in]  address
 *     Partially- or fully-specified address
 * @param[in]  pfxlen
 *     Prefix length, printed at end of result, if @a showpfx is @c TRUE.
 *     Also indicates the number of bits of data that @a addr points to
 *     (rounded up to the nearest byte.)
 * @param[in]  showpfxlen
 *     Print prefix length (/pfxlen) at the end of result
 * @param[out] out
 *     Output buffer
 * @param[in]  outsize
 *     Size of the output buffer, in bytes
 *
 * @return 
 *     0 when not able to format string, error value will be stored in
 *     the global variable @c errno; otherwise the size of the string in
 *     @a out, not including the null-terminator character.
 */
size_t
format_ipv6addr_partial (const void *address, size_t pfxlen,
			 boolean showpfxlen, char *out, size_t outsize);

/**
 * @brief
 * Formats the IPv6 address using the ::-notation for compressing away a
 * sequence of low-order zeroes.
 *
 * @note This function formats the address @b backwards, from the end
 * of the supplied buffer.
 *
 * @param[in] vaddr
 *     Address to format
 * @param[in] plen
 *     Prefix length
 * @param[out] buf
 *     Output buffer
 * @param[in] buflen
 *     Length of the output buffer
 * @param[in] null_terminate
 *     Flag to indicate if the output buffer should be null-terminated
 *
 * @return 
 *     The start of the formatted address string in @a buf.
 */
char *
format_in6_addr (const void *vaddr, int plen, char *buf, int buflen,
		 int null_terminate);

/**
 * @brief
 * Formats the IPv6 prefix into the buffer, using the ::-notation
 * for compressing away a sequence of low-order zeroes.
 *
 * @note This function formats the address @b backwards, from the end
 * of the supplied buffer.
 *
 * @param[in] vaddr
 *     Address to format
 * @param[in] plen
 *     Prefix length
 * @param[out] buf
 *     Output buffer
 * @param[in] buflen
 *     Length of the output buffer
 * @param[in] null_terminate
 *     Flag to indicate if the output buffer should be null-terminated
 * 
 * @return 
 *     The start of the formatted address string in @a buf.
 */
char *
format_in6_prefix (const void *vaddr, int plen, char *buf, int buflen,
		   int null_terminate);

/**
 * @brief
 * Verifies that the IPv4 address is a valid host address.
 * 
 * The address cannot be a multicast or broadcast address.
 *
 * @note Loopback address is not a valid host address, but it is not 
 * checked for.  Historically, we allowed it in earlier releases of
 * JUNOS and enforcing the check may break the customer config during
 * an upgrade.
 *
 * @param[in] address
 *     IPv4 address as a string
 *
 * @return 
 *     @c TRUE if @a address is a valid host address; @c FALSE otherwise.
 */
boolean
valid_ipv4_hostaddr (const char* address);

/**
 * @brief
 * Verifies that the IP address is one of the well known unicast addresses.
 *
 * The address cannot be a multicast, broadcast address or invalid.
 *
 * @note Loopback address is considered a valid unicast address.
 *
 * @param[in] af
 *     Address family. Only IPv4 and IPv6 are supported.
 *
 * @param[in] address
 *     IP address.
 *
 * @return
 *     @c TRUE if @a address is a valid unicast address; @c FALSE otherwise.
 */
boolean
parse_ipaddr_is_unicast (int af, void *address);

/**
 * @brief
 * Returns the type of IPv6 Address i.e. link-local, site-local or 
 * global or multicast
 *
 * @note Loopback address is considered a valid unicast address.
 *
 * @param[in] address
 *     IPv6 address.
 *
 * @return
 *     type of IPv6 address.
 */

ipv6addr_type_t
ipv6_address_type(void *address);
#ifdef __cplusplus
    }
}
#endif /* __cplusplus */

#endif /* __JNX_PARSE_IP_H__ */

