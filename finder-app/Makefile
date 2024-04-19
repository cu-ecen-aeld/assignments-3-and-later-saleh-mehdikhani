# Toolchain and its configuration
CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = 

ifdef CROSS_COMPILE
	CC := $(CROSS_COMPILE)gcc
endif

# Set the target and source files
TARGET = writer
SRC = writer.c
OBJ = $(SRC:.c=.o)

# Default target
.PHONY: all
all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -rf $(TARGET) $(OBJ)

.PHONY: all clean