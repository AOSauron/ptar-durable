CC = gcc -W -Wall -Wno-deprecated-declarations
OBJECTS = main.o

all: ptar

ptar: $(OBJECTS)
	$(CC) -o $@ $(OBJECTS)

main.o: main.c header.h
	$(CC) -c $<

clean:
	rm -rf *.o

mrproper: clean
	rm -rf ptar
