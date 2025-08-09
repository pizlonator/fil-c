
#include <libbz3.h>
#include <stdio.h>
#include <stdlib.h>

#define MB (1024 * 1024)

int main(void) {
    printf("Compressing shakespeare.txt back and forth in memory.\n");

    // Read the entire "shakespeare.txt" file to memory:
    FILE * fp = fopen("shakespeare.txt", "rb");
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char * buffer = malloc(size);
    fread(buffer, 1, size, fp);
    fclose(fp);

    // Compress the file:
    size_t out_size = bz3_bound(size);
    char * outbuf = malloc(out_size);
    int bzerr = bz3_compress(1 * MB, buffer, outbuf, size, &out_size);
    if (bzerr != BZ3_OK) {
        printf("bz3_compress() failed with error code %d", bzerr);
        return 1;
    }

    printf("%d => %d\n", size, out_size);

    // Decompress the file.
    bzerr = bz3_decompress(outbuf, buffer, out_size, &size);
    if (bzerr != BZ3_OK) {
        printf("bz3_decompress() failed with error code %d", bzerr);
        return 1;
    }

    printf("%d => %d\n", out_size, size);

    free(buffer);
    free(outbuf);
    return 0;
}
