all: bug

bug: bug.c ../../libcap Makefile
	$(MAKE) -C ../../libcap
	$(CC) $(CFLAGS) $(CPPFLAGS) -g -I../../libcap/include --static -o $@ $< -L../../libcap -lcap
	./bug

clean:
	rm -f bug.o bug
