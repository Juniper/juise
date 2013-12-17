#
# New ports collection makefile for juise
#

PORTNAME=	juise
PORTVERSION=	0.5.9
PORTREVISION?=	1
CATEGORIES?=	textproc
MASTER_SITES=	https://github.com/Juniper/juise/releases/download/0.5.9/
DIST_SUBDIR=	textproc

MAINTAINER?=	juise@googlegroups.com
COMMENT?=       juise: the JUNOS User Interface Scripting Environment

FETCH_ARGS?=    -Fpr

.if !defined(REFERENCE_PORT)

USE_GMAKE=	yes
GNU_CONFIGURE=	yes
USE_LDCONFIG=	yes
USE_GNOME?=	libxml2 libxslt
CONFIGURE_ARGS?= 

CPPFLAGS+=	-I${LOCALBASE}/include
LDFLAGS+=	-L${LOCALBASE}/lib

.if defined(MASTERDIR)
_SLAVE_PORT=	yes
.else
MAN1=		juise.1x

.endif

.include <bsd.port.pre.mk>

.include <bsd.port.post.mk>

.endif
