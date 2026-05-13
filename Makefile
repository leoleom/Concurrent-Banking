
CC     = gcc
CFLAGS = -Wall -Wextra -pthread -Iinclude
SRC    = src/main.c src/bank.c src/buffer_pool.c src/timer.c \
         src/transaction.c src/lock_mgr.c src/metrics.c src/utils.c\
		 src/parser.c
TARGET = bankdb
 
.PHONY: all debug clean
 
all: $(TARGET)
 
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^
 
debug: $(SRC)
	$(CC) $(CFLAGS) -g -fsanitize=thread -o $(TARGET)_debug $^

test:

 
clean:
	rm -f $(TARGET) $(TARGET)_debug

