PROG=	statbar
SRCS=	main.c
MAN=

CFLAGS += -I/usr/X11R6/include
LDFLAGS += -L/usr/X11R6/lib -lX11

.include <bsd.prog.mk>

