#include <ctype.h>
#include <errno.h>
extern "C" {
#include <lua.h>
#include <lauxlib.h>
extern int tsort(lua_State *L);
}
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#define DIR _ff_DIR
#include <ff.h>
#undef DIR
#include <circle/alloc.h>
#include "fs_handle.h"


typedef struct {
    _ff_DIR d;
    FILINFO info;
} DIR;

typedef struct dirent {
	FSIZE_t	fsize;			/* File size */
	WORD	fdate;			/* Modified date */
	WORD	ftime;			/* Modified time */
	BYTE	fattrib;		/* File attribute */
#if FF_USE_LFN
	TCHAR	altname[FF_SFN_BUF + 1];/* Alternative file name */
	TCHAR	d_name[FF_LFN_BUF + 1];	/* Primary file name */
#else
	TCHAR	d_name[12 + 1];	/* File name */
#endif
} dirent_t;

DIR * opendir(const char* path) {
    DIR * d = (DIR*)malloc(sizeof(DIR));
    if (f_opendir(&d->d, path) != FR_OK) {
        free(d);
        return NULL;
    }
    return d;
}

struct dirent * readdir(DIR * d) {
    if (f_readdir(&d->d, &d->info) != FR_OK || d->info.fname[0] == '\0') return NULL;
    return (struct dirent*)&d->info;
}

void closedir(DIR * d) {
    f_closedir(&d->d);
    free(d);
}

int rmdir(const char* path) {
    return f_rmdir(path);
}

char* dirname(char* path) {
    char tch;
    if (strrchr(path, '/') != NULL) tch = '/';
    else if (strrchr(path, '\\') != NULL) tch = '\\';
    else {
        path[0] = '\0';
        return path;
    }
    path[strrchr(path, tch) - path] = '\0';
	return path;
}

const char* basename(const char* path) {
    const char* s = strrchr(path, '/');
    if (s) return s + 1;
    else return path;
}

char * unconst(const char * str) {
    char * retval = (char*)malloc(strlen(str) + 1);
    strcpy(retval, str);
    return retval;
}

static inline bool allchar(const char* str, char c) {
    while (*str) if (*str++ != c) return false;
    return true;
}

char* fixpath(const char* pat) {
    char* path = unconst(pat);
    char* retval = (char*)malloc(strlen(path) + 2);
    retval[0] = 0;
    for (char* tok = strtok(path, "/\\"); tok; tok = strtok(NULL, "/\\")) {
        if (strcmp(tok, "") == 0) continue;
        else if (strcmp(tok, "..") == 0) {
            char* p = strrchr(retval, '/');
            if (p) *p = '\0';
            else strcpy(retval, "..");
        } else if (allchar(tok, '.')) continue;
        else {
            strcat(retval, "/");
            strcat(retval, tok);
        }
    }
    free(path);
    if (strcmp(retval, "") == 0) strcpy(retval, "/");
    return retval;
}

void err(lua_State *L, char * path, const char * err) {
    char * msg = (char*)malloc(strlen(path) + strlen(err) + 3);
    sprintf(msg, "%s: %s", path, err);
    free(path);
    lua_pushstring(L, msg);
    lua_error(L);
}

int fs_list(lua_State *L) {
    struct dirent *dir;
    int i;
    DIR * d;
    char * path = fixpath(lua_tostring(L, 1));
    d = opendir(path);
    if (d) {
        lua_newtable(L);
        for (i = 1; (dir = readdir(d)) != NULL; i++) {
            lua_pushinteger(L, i);
            lua_pushstring(L, dir->d_name);
            lua_settable(L, -3);
        }
        closedir(d);
        // apparently not needed anymore?
        /*if (strcmp(path, "/") == 0) {
            lua_pushinteger(L, i);
            lua_pushliteral(L, "rom");
            lua_settable(L, -3);
            if (diskMounted) {
                lua_pushinteger(L, i);
                lua_pushliteral(L, "disk");
                lua_settable(L, -3);
            }
        }*/
    } else err(L, path, "Not a directory");
    free(path);
    lua_pushcfunction(L, tsort);
    lua_pushvalue(L, -2);
    lua_call(L, 1, 0);
    return 1;
}

int fs_exists(lua_State *L) {
    struct stat st;
    char * path = fixpath(lua_tostring(L, 1));
    lua_pushboolean(L, stat(path, &st) == 0);
    free(path);
    return 1;
}

int fs_isDir(lua_State *L) {
    struct stat st;
    char * path = fixpath(lua_tostring(L, 1));
    lua_pushboolean(L, stat(path, &st) == 0 && S_ISDIR(st.st_mode));
    free(path);
    return 1;
}

static bool isReadOnly(const char* path) {
    if (strcmp(path, "/rom") == 0 || strcmp(path, "rom") == 0) return true;
    else if (strcmp(path, "/disk") == 0 || strcmp(path, "disk") == 0) return false;
    else if (strcmp(path, "/") == 0 || strcmp(path, "") == 0) return false;
    /*if (access(path, F_OK) != 0) {
        char* p = unconst(path);
        bool retval = isReadOnly(dirname(p));
        free(p);
        return retval;
    }
    return access(path, W_OK) != 0;*/
    return false;
}

int fs_isReadOnly(lua_State *L) {
    char * path = fixpath(lua_tostring(L, 1));
    lua_pushboolean(L, isReadOnly(path));
    free(path);
    return 1;
}

int fs_getName(lua_State *L) {
    char * path = unconst(lua_tostring(L, 1));
    lua_pushstring(L, basename(path));
    free(path);
    return 1;
}

int fs_getDrive(lua_State *L) {
    char * path = fixpath(lua_tostring(L, 1));
    if (strncmp(path, "/rom/", 5) == 0) lua_pushliteral(L, "rom");
    else if (strncmp(path, "/disk/", 6) == 0) lua_pushliteral(L, "disk");
    else lua_pushliteral(L, "hdd");
    free(path);
    return 1;
}

int fs_getSize(lua_State *L) {
    struct stat st;
    char * path = fixpath(lua_tostring(L, 1));
    if (stat(path, &st) != 0) err(L, path, "No such file");
    lua_pushinteger(L, st.st_size);
    free(path);
    return 1;
}

size_t capacity_cache[2] = {0, 0}, free_space_cache[2] = {0, 0};

int fs_getFreeSpace(lua_State *L) {
    char * path = fixpath(lua_tostring(L, 1));
    DWORD cap, free;
    if (strncmp(path, "/rom", 4) == 0) free = 0;
    else {
        if (free_space_cache[1]) free = free_space_cache[1];
        else {
            FATFS *fs;
            f_getfree(path, &free, &fs);
            free *= fs->csize * FF_MIN_SS;
            free_space_cache[1] = free;
        }
    }
    lua_pushinteger(L, free);
    return 1;
}

int fs_getCapacity(lua_State *L) {
    char * path = fixpath(lua_tostring(L, 1));
    size_t cap;
    if (strncmp(path, "/rom", 4) == 0) cap = 0;
    else {
        if (capacity_cache[1]) cap = capacity_cache[1];
        else {
            FATFS *fs;
            DWORD free;
            f_getfree(path, &free, &fs);
            cap = (fs->n_fatent - 2) * fs->csize * FF_MIN_SS;
            capacity_cache[1] = cap;
        }
    }
    lua_pushinteger(L, cap);
    return 1;
}

int recurse_mkdir(const char * path) {
    char* d = unconst(path);
    struct stat st;
    dirname(d);
    if (strcmp(path, "") != 0 && strcmp(d, "") != 0 && stat(d, &st) != 0) {
        int err = recurse_mkdir(d);
        if (err) {
            free(d);
            return err;
        }
    }
    free(d);
    return mkdir(path, 0777);
}

int fs_makeDir(lua_State *L) {
    char * path = fixpath(lua_tostring(L, 1));
    if (recurse_mkdir(path) != 0) err(L, path, strerror(errno));
    free(path);
    free_space_cache[0] = free_space_cache[1] = 0;
    return 0;
}

int fs_move(lua_State *L) {
    char * fromPath, *toPath;
    fromPath = fixpath(lua_tostring(L, 1));
    toPath = fixpath(lua_tostring(L, 2));
    char* dir = unconst(toPath);
    if (strcmp(dirname(dir), "") != 0) recurse_mkdir(dir);
    free(dir);
    if (rename(fromPath, toPath) != 0) {
        free(toPath);
        err(L, fromPath, strerror(errno));
    }
    free(fromPath);
    free(toPath);
    return 0;
}

static int aux_copy(const char* fromPath, const char* toPath) {
    static char tmp[1024];
    FILE * fromfp, *tofp;
    int read;
    struct stat st;
    if (stat(fromPath, &st) != 0) return -1;
    if (S_ISDIR(st.st_mode)) {
        struct dirent *dir;
        int i;
        DIR * d;
        d = opendir(fromPath);
        if (d) {
            for (i = 1; (dir = readdir(d)) != NULL; i++) {
                char* fp = (char*)malloc(strlen(fromPath) + strlen(dir->d_name) + 2);
                char* tp = (char*)malloc(strlen(toPath) + strlen(dir->d_name) + 2);
                strcpy(fp, fromPath);
                strcat(fp, "/");
                strcat(fp, dir->d_name);
                strcpy(tp, toPath);
                strcat(tp, "/");
                strcat(tp, dir->d_name);
                int err = aux_copy(fp, tp);
                free(fp);
                free(tp);
                if (err) return err;
            }
            closedir(d);
        } else return -1;
    } else {
        char* dir = unconst(toPath);
        if (strcmp(dirname(dir), "") != 0) recurse_mkdir(dir);
        free(dir);
        fromfp = fopen(fromPath, "r");
        if (fromfp == NULL) {
            return -1;
        }
        tofp = fopen(toPath, "w");
        if (tofp == NULL) {
            fclose(fromfp);
            return -1;
        }

        while (!feof(fromfp)) {
            read = fread(tmp, 1, 1024, fromfp);
            fwrite(tmp, read, 1, tofp);
            if (read < 1024) break;
        }

        fclose(fromfp);
        fclose(tofp);
    }
    return 0;
}

int fs_copy(lua_State *L) {
    char* fromPath = fixpath(lua_tostring(L, 1));
    char* toPath = fixpath(lua_tostring(L, 2));
    if (aux_copy(fromPath, toPath)) {
        free(toPath);
        err(L, fromPath, "Failed to copy");
    }
    free(fromPath);
    free(toPath);
    free_space_cache[0] = free_space_cache[1] = 0;
    return 0;
}

int aux_delete(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    if (S_ISDIR(st.st_mode)) {
        struct dirent *dir;
        int i;
        DIR * d;
        int ok = 0;
        d = opendir(path);
        if (d) {
            for (i = 1; (dir = readdir(d)) != NULL; i++) {
                char* p = (char*)malloc(strlen(path) + strlen(dir->d_name) + 2);
                strcpy(p, path);
                strcat(p, "/");
                strcat(p, dir->d_name);
                ok = aux_delete(p) || ok;
                free(p);
            }
            closedir(d);
            rmdir(path);
            return ok;
        } else return -1;
    } else return unlink(path);
}

int fs_delete(lua_State *L) {
    char * path = fixpath(lua_tostring(L, 1));
    if (aux_delete(path) != 0) err(L, path, strerror(errno));
    free(path);
    free_space_cache[0] = free_space_cache[1] = 0;
    return 0;
}

int fs_combine(lua_State *L) {
    char * basePath = unconst(luaL_checkstring(L, 1));
    for (int i = 2; i <= lua_gettop(L); i++) {
        if (!lua_isstring(L, i)) {
            free(basePath);
            luaL_checkstring(L, i);
            return 0;
        }
        const char * str = lua_tostring(L, i);
        char* ns = (char*)malloc(strlen(basePath)+strlen(str)+3);
        strcpy(ns, basePath);
        if (strlen(basePath) > 0 && basePath[strlen(basePath)-1] == '/' && str[0] == '/') strcat(ns, str + 1);
        else {
            if (strlen(basePath) && basePath[strlen(basePath)-1] != '/' && str[0] != '/') strcat(ns, "/");
            strcat(ns, str);
        }
        free(basePath);
        basePath = ns;
    }
    char* newPath = fixpath(basePath);
    lua_pushstring(L, newPath + 1);
    free(newPath);
    free(basePath);
    return 1;
}

static int fs_open(lua_State *L) {
    const char * mode = luaL_checkstring(L, 2);
    if ((mode[0] != 'r' && mode[0] != 'w' && mode[0] != 'a') || (mode[1] != 'b' && mode[1] != '\0')) luaL_error(L, "%s: Unsupported mode", mode);
    char * path = fixpath(luaL_checkstring(L, 1));
    struct stat st;
    int err = stat(path, &st);
    if (err != 0 && mode[0] == 'r') {
        lua_pushnil(L);
        lua_pushfstring(L, "%s: No such file", path);
        free(path);
        return 2;
    } else if (err == 0 && S_ISDIR(st.st_mode)) { 
        lua_pushnil(L);
        if (strcmp(mode, "r") == 0 || strcmp(mode, "rb") == 0) lua_pushfstring(L, "%s: No such file", path);
        else lua_pushfstring(L, "%s: Cannot write to directory", path);
        free(path);
        return 2; 
    }
    if (mode[0] == 'w' || mode[0] == 'a') {
        char* dir = unconst(path);
        if (strcmp(dirname(dir), "") != 0) recurse_mkdir(dir);
        free(dir);
    }
    FILE ** fp = (FILE**)lua_newuserdata(L, sizeof(FILE*));
    int fpid = lua_gettop(L);
    *fp = fopen(path, mode);
    if (*fp == NULL) { 
        lua_remove(L, fpid);
        lua_pushnil(L);
        lua_pushfstring(L, "%s: No such file", path);
        free(path);
        return 2; 
    }
    lua_createtable(L, 0, 4);
    lua_pushstring(L, "close");
    lua_pushvalue(L, fpid);
    lua_pushcclosure(L, fs_handle_close, 1);
    lua_settable(L, -3);
    if (strcmp(mode, "r") == 0 || strcmp(mode, "rb") == 0) {
        lua_pushstring(L, "readAll");
        lua_pushvalue(L, fpid);
        lua_pushcclosure(L, fs_handle_readAllByte, 1);
        lua_settable(L, -3);

        lua_pushstring(L, "readLine");
        lua_pushvalue(L, fpid);
        lua_pushboolean(L, false);
        lua_pushcclosure(L, fs_handle_readLine, 2);
        lua_settable(L, -3);

        lua_pushstring(L, "read");
        lua_pushvalue(L, fpid);
        lua_pushcclosure(L, strcmp(mode, "rb") == 0 ? fs_handle_readByte : fs_handle_readChar, 1);
        lua_settable(L, -3);

        lua_pushstring(L, "seek");
        lua_pushvalue(L, fpid);
        lua_pushcclosure(L, fs_handle_seek, 1);
        lua_settable(L, -3);
    } else if (strcmp(mode, "w") == 0 || strcmp(mode, "a") == 0 || strcmp(mode, "wb") == 0 || strcmp(mode, "ab") == 0) {
        lua_pushstring(L, "write");
        lua_pushvalue(L, fpid);
        lua_pushcclosure(L, fs_handle_writeByte, 1);
        lua_settable(L, -3);

        lua_pushstring(L, "writeLine");
        lua_pushvalue(L, fpid);
        lua_pushcclosure(L, fs_handle_writeLine, 1);
        lua_settable(L, -3);

        lua_pushstring(L, "flush");
        lua_pushvalue(L, fpid);
        lua_pushcclosure(L, fs_handle_flush, 1);
        lua_settable(L, -3);

        lua_pushstring(L, "seek");
        lua_pushvalue(L, fpid);
        lua_pushcclosure(L, fs_handle_seek, 1);
        lua_settable(L, -3);
    } else {
        // This should now be unreachable, but we'll keep it here for safety
        fclose(*fp);
        lua_remove(L, fpid);
        free(path);
        luaL_error(L, "%s: Unsupported mode", mode);
    }
    free(path);
    return 1;
}

int fs_getDir(lua_State *L) {
    char * path = unconst(lua_tostring(L, 1));
    dirname(path);
    lua_pushstring(L, path[0] == '/' ? path + 1 : path);
    free(path);
    return 1;
}

#define st_time_ms(st) ((st##tim.tv_nsec / 1000000) + (st##time * 1000.0))

int fs_attributes(lua_State *L) {
    const char* path = fixpath(lua_tostring(L, 1));
    struct stat st;
    if (stat(path, &st) != 0) {
        lua_pushnil(L);
        return 1;
    }
    lua_createtable(L, 0, 6);
    lua_pushinteger(L, st_time_ms(st.st_m));
    lua_setfield(L, -2, "modification");
    lua_pushinteger(L, st_time_ms(st.st_m));
    lua_setfield(L, -2, "modified");
    lua_pushinteger(L, st_time_ms(st.st_c));
    lua_setfield(L, -2, "created");
    lua_pushinteger(L, S_ISDIR(st.st_mode) ? 0 : st.st_size);
    lua_setfield(L, -2, "size");
    lua_pushboolean(L, S_ISDIR(st.st_mode));
    lua_setfield(L, -2, "isDir");
    lua_pushboolean(L, isReadOnly(path));
    lua_setfield(L, -2, "isReadOnly");
    free((void*)path);
    return 1;
}

extern "C" {
extern const luaL_Reg fs_lib[] = {
    {"list", fs_list},
    {"exists", fs_exists},
    {"isDir", fs_isDir},
    {"isReadOnly", fs_isReadOnly},
    {"getName", fs_getName},
    {"getDrive", fs_getDrive},
    {"getSize", fs_getSize},
    {"getFreeSpace", fs_getFreeSpace},
    {"getCapacity", fs_getCapacity},
    {"makeDir", fs_makeDir},
    {"move", fs_move},
    {"copy", fs_copy},
    {"delete", fs_delete},
    {"combine", fs_combine},
    {"open", fs_open},
    {"getDir", fs_getDir},
    {"attributes", fs_attributes},
    {NULL, NULL}
};
}
