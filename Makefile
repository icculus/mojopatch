
# Quick Makefile by ryan c. gordon. (icculus@clutteredmind.org)

CC := gcc
LD := gcc
BINDIR := bin
SRCDIR := .

# must be "macosx" or "unix" or "win32" ... not all necessarily work right now.
platform := macosx

# Add zlib support? Will compress all ADD/ADDORREPLACE/PATCH operations.
# If you're going to compress the patch anyhow, this might not be wanted.
use_zlib := false

# Unix/Mac will try fork() if this is false.
use_pthread := false


# you probably shouldn't touch anything below this line.

ifeq ($(strip $(platform)),macosx)
PLATFORMDEF := -DPLATFORM_UNIX=1 -DPLATFORM_MACOSX=1
PLATFORMSRCS := platform_unix.c
LDFLAGS := -framework Carbon
endif

ifeq ($(strip $(platform)),win32)
PLATFORMDEF := -DPLATFORM_WIN32=1
PLATFORMSRCS := platform_win32.c
endif

ifeq ($(strip $(platform)),unix)
  PLATFORMDEF := -DPLATFORM_UNIX=1
  PLATFORMSRCS := platform_unix.c
endif

#CFLAGS := $(PLATFORMDEF) -Wall -g -fsigned-char -fno-omit-frame-pointer -O0 -DDEBUG=1 -D_DEBUG=1
CFLAGS := $(PLATFORMDEF) -Wall -fsigned-char -fomit-frame-pointer -Os

ifeq ($(strip $(platform)),macosx)
  CFLAGS += -mdynamic-no-pic
endif

ifeq ($(strip $(use_zlib)),true)
  CFLAGS += -DUSE_ZLIB
  LDFLAGS += -lz
endif

ifeq ($(strip $(use_pthread)),true)
  CFLAGS += -DUSE_PTHREAD=1
  ifneq ($(strip $(platform)),macosx)
    LDFLAGS += -lpthread
  endif
endif

CFLAGS += $(EXTRACFLAGS)
LDFLAGS += $(EXTRALDFLAGS)

MOJOPATCHSRCS := mojopatch.c md5.c ui.c ui_carbon.c ui_stdio.c $(PLATFORMSRCS)
OBJS1 := $(MOJOPATCHSRCS:.c=.o)
OBJS2 := $(OBJS1:.cpp=.o)
OBJS3 := $(OBJS2:.asm=.o)
OBJS4 := $(OBJS3:.m=.o)
MOJOPATCHOBJS := $(foreach f,$(OBJS4),$(BINDIR)/$(f))
MOJOPATCHSRCS := $(foreach f,$(MOJOPATCHSRCS),$(SRCDIR)/$(f))

.PHONY: all mojopatch clean distclean listobjs listsrcs

all : mojopatch

mojopatch : $(BINDIR)/mojopatch

$(BINDIR)/%.o: $(SRCDIR)/%.m
	$(CC) -c -o $@ $< $(CFLAGS)

$(BINDIR)/%.o: $(SRCDIR)/%.cpp
	$(CC) -c -o $@ $< $(CFLAGS)

$(BINDIR)/%.o: $(SRCDIR)/%.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(BINDIR)/mojopatch : $(BINDIR) $(MOJOPATCHOBJS)
	$(LD) $(LDFLAGS) -o $@ $(MOJOPATCHOBJS)

$(BINDIR):
	mkdir -p $(BINDIR)

distclean : clean

clean:
	rm -rf $(BINDIR)

listsrcs:
	@echo $(MOJOPATCHSRCS)

listobjs:
	@echo $(MOJOPATCHOBJS)


# end of Makefile ...

