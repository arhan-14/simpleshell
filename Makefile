CC = gcc
CFLAGS = -Wall -fsanitize=address -g

TARGET = mysh
SRCS = mysh.c parse.c exec.c
OBJS = $(SRCS:.c=.o)
HDRS = mysh.h

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
