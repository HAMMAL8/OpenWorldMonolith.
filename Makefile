
TARGET = EBOOT
OBJS = main.o

CFLAGS = -O2 -G0 -Wall
LDFLAGS =

PSP_FW_VERSION = 150
BUILD_PRX = 1

LIBS = -lpspdebug -lpspctrl -lpspkernel -lm

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
