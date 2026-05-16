CC       ?= gcc
CFLAGS   := -O2 -Wall -Wextra -Wno-unused-parameter -std=gnu11
CFLAGS   += $(shell pkg-config --cflags libdrm cairo libinput xkbcommon)
LDFLAGS  :=
LIBS     := $(shell pkg-config --libs libdrm cairo libinput xkbcommon) -lpam -ludev -lm

PREFIX   ?= /usr/local
BINDIR   := $(PREFIX)/sbin
PAMDIR   := /etc/pam.d
INITDIR  := /etc/init.d

SRCDIR   := src
INCDIR   := include
OBJDIR   := build

SOURCES  := main.c \
            $(SRCDIR)/drm.c \
            $(SRCDIR)/input.c \
            $(SRCDIR)/renderer.c \
            $(SRCDIR)/widgets.c \
            $(SRCDIR)/auth.c \
            $(SRCDIR)/session.c \
            $(SRCDIR)/ui_state.c

OBJECTS  := $(patsubst %.c,$(OBJDIR)/%.o,$(SOURCES))
TARGET   := snowfall

.PHONY: all clean install uninstall

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

$(OBJDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(INCDIR) -c $< -o $@

$(OBJDIR)/$(SRCDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(INCDIR) -c $< -o $@

clean:
	rm -rf $(OBJDIR) $(TARGET)

install: $(TARGET)
	install -Dm755 $(TARGET)       $(DESTDIR)$(BINDIR)/$(TARGET)
	install -Dm644 snowfall.pam    $(DESTDIR)$(PAMDIR)/snowfall
	install -Dm755 snowfall.openrc $(DESTDIR)$(INITDIR)/snowfall

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)
	rm -f $(DESTDIR)$(PAMDIR)/snowfall
	rm -f $(DESTDIR)$(INITDIR)/snowfall
