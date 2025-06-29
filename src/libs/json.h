#ifndef __JSON_H__
#define __JSON_J__
#include <jansson.h>
#include "util.h"

#define __ALLOC_JSON_NEW(name, ...)             json name; __ALLOC_JSON(name, __VA_ARGS__)
#define __ALLOC_JSON(name, ...)                 name.data = json_pack(__VA_ARGS__); name.ref = TRUE; if (name.data)
#define __ALLOC_FAIL                            else
#define __FREE_JSON(name)                       free_json(&name);
#define __FREE_JSONS(...)                       free_jsons(CNT_ARGS(__VA_ARGS__), AMPS(__VA_ARGS__))
#define __UNPACK_JSON(target, ...)              if (json_unpack(target.data, __VA_ARGS__) >= 0)
#define __UNPACK_FAIL                           else

#define IS_NULL(name)                           json_is_null(name)

typedef struct {
    json_t*             data; // real json object
    bool                ref; // referred or not
} json;

void free_json(json*);
void free_jsons(int, ...);
#endif