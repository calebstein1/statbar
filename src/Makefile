PROG=	statbar
SRCS=	main.c glyphs.c
MAN=

CFLAGS += -I../include -I/usr/X11R6/include -I/usr/local/include
LDFLAGS += -L/usr/local/lib -L/usr/X11R6/lib -L../modules -lsndio -lX11 -lcurl -lpthread -lstatbar_modules

.include <bsd.prog.mk>

