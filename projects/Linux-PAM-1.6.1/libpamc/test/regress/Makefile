CFLAGS = -g -I ../../include

test.libpamc: test.libpamc.o
	$(CC) -o $@ $(CFLAGS) $< -L ../.. -lpamc

clean:
	rm -f test.libpamc test.libpamc.o
