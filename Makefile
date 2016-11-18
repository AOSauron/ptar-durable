CC = gcc
LDFLAGS = -rdynamic -ldl
CFLAGS = -Wall -Wextra
OBJECTS = main.o checkfile.o utils.o

all: ptar

ptar: $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(OBJECTS) $(LDFLAGS)

main.o: main.c header.h checkfile.h utils.h
	$(CC) $(CFLAGS) -c $<

checkfile.o: checkfile.c
	$(CC) $(CFLAGS) -c $<

utils.o: utils.c header.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm -rf *.o

mrproper: clean
	rm -rf ptar
