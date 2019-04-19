MKDIR_P := mkdir -p
CP := cp
MV := mv
CC := gcc
CCC := g++
RM_RF = rm -rf
CURL := curl -L
TAR := tar
DIRNAME := dirname
# directories
CWD := .
BINDIR := $(CWD)/bin
BUILDDIR := $(CWD)/build
SRCDIR := $(CWD)/src
INCLUDEDIR := $(CWD)/include

# flags
CUTOUTS :=
OPTIMISATIONS := -O2
CFLAGS := -I$(INCLUDEDIR) -D COLOR -Wall -ggdb3 -Wextra -std=gnu11 $(OPTIMISATIONS) $(CUTOUTS)
LDFLAGS := -pthread -ljson-c -lvlc -lm

# target files
DIRS_TARGET := $(BINDIR) $(BUILDDIR)
TARGET := $(BINDIR)/host
SRCFILES := $(shell find $(SRCDIR) -name '*.c')
OBJFILES_INT = $(subst $(SRCDIR)/,$(BUILDDIR)/,$(SRCFILES))
OBJFILES := $(subst .c,.o,$(OBJFILES_INT))

# fancy targets
all:  directories  $(TARGET)

directories: $(DIRS_TARGET)

# less fancy targets

$(DIRS_TARGET):
	$(MKDIR_P) $@

$(TARGET): $(OBJFILES)
	$(CC) $(LDFLAGS) -o "$@"  $^
 
$(OBJFILES): $(SRCFILES)
	$(MKDIR_P) $(@D)
	$(CC) $(CFLAGS) -c -o "$@" $(subst build/,src/,$(subst .o,.c,$@))

clean:                                                                          
	$(RM_RF) $(DIRS_TARGET)     

execute: $(TARGET)
	$^
