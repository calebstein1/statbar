LIB=	statbar_modules
SRCS=	clock.c battery.c volume.c weather.c mail.c
MAN=

CFLAGS += -I../include -I/usr/local/include
LDFLAGS += -L/usr/local/lib -lsndio -lpthread -lcurl

.include <bsd.lib.mk>
