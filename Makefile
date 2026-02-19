SUBDIR=	modules src
PROG=	statbar
MAN=

CFLAGS += -I/usr/X11R6/include -I/usr/local/include
LDFLAGS += -L/usr/X11R6/lib -lsndio -lX11

.include <bsd.subdir.mk>

