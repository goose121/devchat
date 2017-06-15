SRCS=devchat.c
KMOD=devchat
EXPORT_SYMS=YES
DEBUG_FLAGS= -g

.include <bsd.kmod.mk>
