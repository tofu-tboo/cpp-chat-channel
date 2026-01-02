#ifndef __JSON_H__
#define __JSON_H__
#include <memory>
#include <jansson.h>
#include "util.h"

#define __ALLOC_JSON_NEW(name, ...)             json name = NULL; __ALLOC_JSON(name, __VA_ARGS__)
#define __ALLOC_JSON(name, ...)                 name = json_pack(__VA_ARGS__); if (name)
#define __ALLOC_FAIL                            else
// #define __FREE_JSON(name)                       free_json(name);
// #define __FREE_JSONS(...)                       free_jsons(CNT_ARGS(__VA_ARGS__), __VA_ARGS__)
#define __UNPACK_JSON(target, ...)              if (json_unpack((target).get(), __VA_ARGS__) >= 0)
#define __UNPACK_FAIL                           else

#define IS_NULL(name)                           json_is_null(name)

typedef json_t* json;

void free_json(json);
void free_jsons(int, ...);

struct JsonDeleter {
    void operator()(json p) { free_json(p); }
};
struct FreeChar {
    void operator()(char* p) const { free(p); }
};

typedef std::unique_ptr<json_t, JsonDeleter> Json;
typedef std::unique_ptr<char, FreeChar> CharDump;

#endif
