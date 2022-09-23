CC=gcc
CFLAGS=-I.
DEPS = gpio.h
DEPS = fintek_api.h

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

gpio: gpio.o main.o
	gcc -fPIC main.o gpio.o libfintek_api.a -pthread -o IOInterface -I.
clean:
	rm IOInterface main.o gpio.o

