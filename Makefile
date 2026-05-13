
CC     = gcc
CFLAGS = -Wall -Wextra -pthread -Iinclude
SRC    = src/main.c src/bank.c src/buffer_pool.c src/timer.c \
         src/transaction.c src/parser.c src/lock_mgr.c src/metrics.c src/utils.c
TARGET = bankdb
 
.PHONY: all debug clean
 
all: $(TARGET)
 
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^
 
debug: $(SRC)
	$(CC) $(CFLAGS) -g -fsanitize=thread -o $(TARGET)_debug $^

test:
	@echo "Running test: trace.txt (basic scenario)"
	./bankdb --accounts=tests/accounts.txt --trace=tests/trace.txt --tick-ms=100 --verbose

	@echo "Running test: trace_simple.txt"
	./bankdb --accounts=tests/accounts.txt --trace=tests/trace_simple.txt --tick-ms=100 --verbose

	@echo "Running test: trace_abort.txt"
	./bankdb --accounts=tests/accounts.txt --trace=tests/trace_abort.txt --tick-ms=100 --verbose

	@echo "Running test: trace_buffer.txt"
	./bankdb --accounts=tests/accounts.txt --trace=tests/trace_buffer.txt --tick-ms=100 --verbose

	@echo "Running test: trace_deadlock.txt"
	./bankdb --accounts=tests/accounts.txt --trace=tests/trace_deadlock.txt --tick-ms=100 --verbose

	@echo "Running test: trace_readers.txt"
	./bankdb --accounts=tests/accounts.txt --trace=tests/trace_readers.txt --tick-ms=100 --verbose

	@echo "All test cases executed."

clean:
	rm -f $(TARGET) $(TARGET)_debug
