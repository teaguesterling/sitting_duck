#include <stdio.h>

struct reader {
    FILE *fp;
    long (*seek_fn)(FILE *, long);
};

static long check_offset(long offset) {
    return offset >= 0 ? offset : -1;
}

long read_block(struct reader *r, long offset) {
    if (fseeko(r->fp, check_offset(offset), 0) != 0) {
        return -1;
    }
    return ftello(r->fp);
}

long skip_block(struct reader *r, long offset) {
    if (fseeko(r->fp, offset, 1) != 0) {
        return -1;
    }
    return r->seek_fn(r->fp, offset);
}

int main(void) {
    struct reader r;
    long pos = read_block(&r, 100);
    printf("%ld\n", pos);
    return (int)skip_block(&r, pos);
}
