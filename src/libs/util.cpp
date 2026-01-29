#include "util.h"

void frees(int num, ...) {
    va_list ap;
    va_start(ap, num);

    int i;
    for (i = 0; i < num; i++) {
        void* ptr = va_arg(ap, void*);
        free(ptr); // no needed nullptr check
    }
    va_end(ap);
}

const coded_runtime_error* try_get_coded_error(const std::exception& e) {
    return dynamic_cast<const coded_runtime_error*>(&e);
}