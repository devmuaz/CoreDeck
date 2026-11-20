/* Declared here so a glibc macro cannot rewrite this definition into res_search. */
int res_search(const char *name, int cls, int type, unsigned char *answer, int anslen);

int __res_search(const char *name, int cls, int type, unsigned char *answer, int anslen) {
    return res_search(name, cls, type, answer, anslen);
}
