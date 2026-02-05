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

msec64 now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}