PROG=	statbar
SRCS=	main.c
MAN=

CFLAGS += -I/usr/X11R6/include
LDFLAGS += -L/usr/X11R6/lib -lsndio -lX11 -pthread

.include <bsd.prog.mk>

