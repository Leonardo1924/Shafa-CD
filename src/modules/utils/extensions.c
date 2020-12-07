/************************************************
 *
 *  Author(s): Pedro Tavares
 *  Created Date: 4 Dec 2020
 *  Updated Date: 7 Dec 2020
 *
 ***********************************************/


#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "extensions.h"

// If used strrchr the worst case would be a long path without a '.' because it would compare every letter
bool check_ext(const char * path, const char * const ext)
{
    size_t diff;

    if (path) {

        diff = strlen(path) - strlen(ext);

        if (diff >= 0) {
            path += diff;

            return (!strcmp(path, ext));
        }
    }

    return false;
}

char * add_ext(char * path, const char * const ext, const bool clone)
{
    size_t len_path = strlen(path);
    size_t len_ext = strlen(ext);
    char * new_path;

    if (clone) {
        new_path = malloc(len_path + len_ext + 1);

        if (new_path)
            memcpy(new_path, path, len_path);
    }
    else
        new_path = realloc(path, len_path + len_ext + 1);

    if (new_path)
        memcpy(new_path + len_path, ext, len_ext + 1);

    return new_path;
}


char * rm_ext(char * const path)
{
    char * path_ext = strrchr(path, '.');

    if (path_ext)
        *path_ext = '\0';

    return path;
}