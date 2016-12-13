CC = gcc
LDFLAGS = -lpthread -rdynamic -ldl
CFLAGS = -Wall -Wextra
OBJECTS = main.o checkfile.o utils.o

all: ptar

ptar: $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(OBJECTS) $(LDFLAGS)
	rm *.o

main.o: src/main.c src/utils.h
	$(CC) $(CFLAGS) -c $<

checkfile.o: src/checkfile.c src/utils.h
	$(CC) $(CFLAGS) -c $<

utils.o: src/utils.c src/utils.h src/checkfile.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm -rf *.o

mrproper: clean
	rm -rf ptar
