# Toolchain and its configuration
CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -Wall -Wextra -g
LDFLAGS ?= -lpthread

# Set the target and source files
TARGET = aesdsocket 
SRC = aesdsocket.c
OBJ = $(SRC:.c=.o)

# Default target
.PHONY: all
all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -rf $(TARGET) $(OBJ)

.PHONY: all clean
