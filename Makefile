CC = gcc
LDFLAGS = -lpthread -rdynamic -ldl
CFLAGS = -Wall -Wextra
OBJECTS = main.o checkfile.o utils.o #sorting.o

all: ptar

ptar: $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(OBJECTS) $(LDFLAGS)

main.o: main.c utils.h
	$(CC) $(CFLAGS) -c $<

checkfile.o: checkfile.c utils.h
	$(CC) $(CFLAGS) -c $<

utils.o: utils.c utils.h checkfile.h
	$(CC) $(CFLAGS) -c $<

#sorting.o: sorting.c utils.h sorting.h
#	$(CC) $(CFLAGS) -c $<

clean:
	rm -rf *.o

mrproper: clean
	rm -rf ptar
