MKDIR_P := mkdir -p
CP := cp
MV := mv
CC := gcc
CCC := g++
RM_RF = rm -rf
CURL := curl -L
TAR := tar
# directories
CWD := $(realpath .)
BINDIR := $(CWD)/bin
BUILDDIR := $(CWD)/build
SRCDIR := $(CWD)/src
INCLUDEDIR := $(CWD)/include

# flags
CFLAGS := -I$(INCLUDEDIR) -D NOSER -D COLOR -Wall -ggdb3 -Wextra
LDFLAGS := -pthread -ljson-c -lvlc

# target files
DIRS_TARGET := $(BINDIR) $(BUILDDIR)
TARGET := $(BINDIR)/host
SRCFILES := $(wildcard $(SRCDIR)/*.c)
OBJFILES := $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SRCFILES))

# fancy targets
all:  directories  $(TARGET)

directories: $(DIRS_TARGET)

# less fancy targets

$(DIRS_TARGET):
	$(MKDIR_P) $@

$(TARGET): $(OBJFILES)
	$(CC) $(LDFLAGS) -o $@  $^
 
$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<                              

clean:                                                                          
	$(RM_RF) $(DIRS_TARGET)     

execute: $(TARGET)
	$^
