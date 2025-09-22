all: \
	fopen_failure.so \
	link_failure.so \
	open_RDONLY_failure.so \
	open_RDWR_failure.so \
	rename_failure.so \
	rmdir_failure.so \
	time_0.so \
	time_past.so \
	unlink_failure.so \
	unlinkat_failure.so

%.so: %.c
	gcc -W -Wall -pedantic -g $< -shared -ldl -o $@
