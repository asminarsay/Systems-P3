CC = gcc
CFLAGS = -Wall -Wextra -g
SANFLAGS = -fsanitize=address,undefined -fno-omit-frame-pointer
LDFLAGS = -lm

TARGET = mysh
SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

san:
	$(CC) $(CFLAGS) $(SANFLAGS) $(SRCS) -o $(TARGET) $(LDFLAGS)

.PHONY: clean san