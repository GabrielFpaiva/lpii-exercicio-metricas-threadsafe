CC     = gcc
CFLAGS = -Wall -Wextra -pthread

all: metrics_test

metrics_test: metrics.c main.c metrics.h
	$(CC) $(CFLAGS) -o $@ metrics.c main.c

run: metrics_test
	./metrics_test

# ThreadSanitizer — substitui o Helgrind no macOS (arm64 nao tem valgrind)
tsan: metrics.c main.c metrics.h
	$(CC) $(CFLAGS) -fsanitize=thread -g -O1 -o metrics_tsan metrics.c main.c
	./metrics_tsan

helgrind: metrics_test
	valgrind --tool=helgrind ./metrics_test

memcheck: metrics_test
	valgrind --leak-check=full ./metrics_test

clean:
	rm -rf metrics_test metrics_tsan *.dSYM tsan.log

.PHONY: all run tsan helgrind memcheck clean
