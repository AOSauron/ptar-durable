CC = gcc
LDFLAGS = -lpthread -rdynamic -ldl
CFLAGS = -Wall -Wextra
OBJECTS = main.o checkfile.o utils.o
PFLAGS = -xlp 4
PZFLAGS = -xlzp 4
GREEN=\033[0;32m
NC=\033[0m # No Color

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
	./scripts/rmtest.sh

mrproper: clean
	rm -rf ptar

test:
	@echo "Test on TAR files ...\n"
	./ptar $(PFLAGS) archives_test/verysimple.tar
	@echo ""
	./ptar $(PFLAGS) archives_test/testf.tar
	@echo ""
	./ptar $(PFLAGS) archives_test/testall.tar
	@echo ""
	./ptar $(PFLAGS) archives_test/vice.tar
	@echo ""
	./ptar $(PFLAGS) archives_test/testlink.tar
	@echo ""
	./ptar $(PFLAGS) archives_test/testlien2.tar
	@echo ""
	@echo "Test a corrupted file ..."
	./ptar $(PFLAGS) archives_test/testcorrupted.tar &> /dev/null
	@sleep 1
	@echo ""
	@echo "${GREEN}Worked${NC}\n"
	@sleep 1
	@echo "Test on TAR.GZ files ..."
	./ptar $(PZFLAGS) archives_test/testf.tar.gz
	@echo ""
	./ptar $(PZFLAGS) archives_test/testall.tar.gz
	@echo ""
	./ptar $(PZFLAGS) archives_test/testallbis.tar.gz
	@echo ""
	./ptar $(PZFLAGS) archives_test/vice.tar.gz
	@echo ""
	./ptar $(PZFLAGS) archives_test/bigarch.tar.gz
	@echo ""
	@echo "Test on a false archive ..."
	./ptar $(PZFLAGS) archives_test/testfalsearch.tar.gz &> /dev/null
	@sleep 1
	@echo ""
	@echo "${GREEN}Worked${NC}\n"
	@echo "Finished all ${GREEN}good${NC}. To clean your folder, run \' make clean\'"
