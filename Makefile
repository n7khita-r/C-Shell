CC = gcc
CFLAGS = -std=c99 \
         -D_POSIX_C_SOURCE=200809L \
         -D_XOPEN_SOURCE=700 \
         -Wall -Wextra -Werror \
         -Wno-unused-parameter \
         -fno-asm \
         -Isrc -Iinclude

SRCDIR = src
INCDIR = include
SOURCES = $(SRCDIR)/shell.c $(SRCDIR)/input.c $(SRCDIR)/parser.c $(SRCDIR)/utils.c $(SRCDIR)/hop.c $(SRCDIR)/executor.c $(SRCDIR)/reveal.c $(SRCDIR)/log.c $(SRCDIR)/bg_jobs.c $(SRCDIR)/activities.c $(SRCDIR)/ping.c $(SRCDIR)/fg.c $(SRCDIR)/bg.c
OBJECTS = $(SOURCES:.c=.o)
TARGET = shell.out

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

$(SRCDIR)/%.o: $(SRCDIR)/%.c $(INCDIR)/shell.h $(INCDIR)/bg_jobs.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

# Additional debugging target
debug: CFLAGS += -g -DDEBUG
debug: $(TARGET)

# Test target (when test script is provided)
test: $(TARGET)
	./test_script.sh
