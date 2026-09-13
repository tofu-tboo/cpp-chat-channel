#include "json.h"

void free_json(json ptr) {
    if (ptr && !IS_NULL(ptr)) {
        json_decref(ptr); // no needed nullptr check
    }
}
void free_jsons(int num, ...) {
    va_list ap;
    va_start(ap, num);

    int i;
    for (i = 0; i < num; i++) {
        json ptr = va_arg(ap, json);
        free_json(ptr);
    }
    va_end(ap);
}
