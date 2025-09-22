all: test_nss libsubid_zzz.so

test_nss: test_nss.c ../../../lib/nss.c
	gcc -c -I../../../lib/ -I../../.. -o test_nss.o test_nss.c
	gcc -o test_nss test_nss.o ../../../lib/.libs/libshadow.a -ldl

libsubid_zzz.so: libsubid_zzz.c
	gcc -c -I../../../lib/ -I../../.. -I../../../libsubid libsubid_zzz.c
	gcc -L../../../libsubid -shared -o libsubid_zzz.so libsubid_zzz.o ../../../lib/.libs/libshadow.a -ldl

clean:
	rm -f *.o *.so test_nss
