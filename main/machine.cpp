#include <string.h>
extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}
#include <stdlib.h>
#include <circle/timer.h>
#include <circle/startup.h>
//#include "driver/wifi.hpp"
#include "event.hpp"
#include "module/terminal.hpp"

static const char* eventNames[] = {
    "",
    "key",
    "key_up",
    "char",
    "timer",
    "alarm",
    "disk",
    "disk_eject",
    "http_check",
    "http_success",
    "http_failure",
    "modem_message",
    "paste",
    "redstone",
    "speaker_audio_empty",
    "terminate",
    "websocket_closed",
    "websocket_failure",
    "websocket_message",
    "websocket_success",
    "wifi_connected",
    "wifi_disconnected",
    "wifi_scan",
};

extern "C" {
extern const luaL_Reg fs_lib[];
extern const luaL_Reg http_lib[];
extern const luaL_Reg os_lib[];
extern const luaL_Reg peripheral_lib[];
extern const luaL_Reg rs_lib[];
extern const luaL_Reg term_lib[];
//extern const luaL_Reg wifi_lib[];
}

lua_State *paramQueue;

static int getNextEvent(lua_State *L, const char * filter) {
    lua_State *event = NULL;
    do {
        if (event) lua_remove(paramQueue, 1);
        while (lua_gettop(paramQueue) == 0) {
            event_t ev;
            event_wait(&ev);
            event = lua_newthread(paramQueue);
            lua_pushstring(event, eventNames[ev.type]);
            switch (ev.type) {
                case EVENT_TYPE_KEY: case EVENT_TYPE_KEY_UP:
                    lua_pushinteger(event, ev.key.keycode);
                    if (ev.type == EVENT_TYPE_KEY) lua_pushboolean(event, ev.key.repeat);
                    break;
                case EVENT_TYPE_CHAR:
                    lua_pushlstring(event, &ev.character.c, 1);
                    break;
                case EVENT_TYPE_TIMER: case EVENT_TYPE_ALARM:
                    lua_pushinteger(event, ev.timer.timerID);
                    break;
                case EVENT_TYPE_DISK: case EVENT_TYPE_DISK_EJECT:
                    lua_pushliteral(event, "right");
                    break;
                case EVENT_TYPE_SPEAKER_AUDIO_EMPTY:
                    lua_pushliteral(event, "left");
                    break;
                case EVENT_TYPE_MODEM_MESSAGE:
                    lua_checkstack(event, 10);
                    lua_pushliteral(event, "back");
                    lua_pushinteger(event, ev.modem.channel);
                    lua_pushinteger(event, ev.modem.replyChannel);
                    ev.modem.message_fn(event, ev.modem.message_arg);
                    lua_pushnumber(event, ev.modem.distance);
                    break;
                case EVENT_TYPE_HTTP_FAILURE: case EVENT_TYPE_HTTP_SUCCESS: case EVENT_TYPE_HTTP_CHECK:
                case EVENT_TYPE_WEBSOCKET_CLOSED: case EVENT_TYPE_WEBSOCKET_FAILURE:
                case EVENT_TYPE_WEBSOCKET_MESSAGE: case EVENT_TYPE_WEBSOCKET_SUCCESS:
                    lua_pushstring(event, ev.http.url);
                    if (ev.type == EVENT_TYPE_HTTP_CHECK) free(ev.http.url);
                    if (ev.http.err) lua_pushstring(event, ev.http.err);
                    if (ev.http.handle_fn) ev.http.handle_fn(event, ev.http.handle_arg);
                    break;
            }
        }
        event = lua_tothread(paramQueue, 1);
    } while (filter != NULL && strcmp(lua_tostring(event, 1), filter) != 0 && strcmp(lua_tostring(event, 1), "terminate") != 0);
    int count = lua_gettop(event);
    lua_xmove(event, L, count);
    lua_remove(paramQueue, 1);
    return count;
}

void machine_main(void*) {
    int status;
    lua_State *L;
    lua_State *coro;
    /*
     * All Lua contexts are held in this structure. We work with it almost
     * all the time.
     */
    L = luaL_newstate();
    
    coro = lua_newthread(L);
    paramQueue = lua_newthread(L);
    
    luaL_openlibs(coro);
    lua_getglobal(coro, "os"); lua_getfield(coro, -1, "date");
    lua_newtable(coro); luaL_setfuncs(coro, fs_lib, 0); lua_setglobal(coro, "fs");
    lua_newtable(coro); luaL_setfuncs(coro, http_lib, 0); lua_setglobal(coro, "http");
    lua_newtable(coro); luaL_setfuncs(coro, os_lib, 0); lua_pushvalue(coro, -2); lua_setfield(coro, -2, "date"); lua_setglobal(coro, "os");
    lua_newtable(coro); luaL_setfuncs(coro, peripheral_lib, 0); lua_setglobal(coro, "peripheral");
    lua_newtable(coro); luaL_setfuncs(coro, rs_lib, 0); lua_pushvalue(coro, -1); lua_setglobal(coro, "rs"); lua_setglobal(coro, "redstone");
    lua_newtable(coro); luaL_setfuncs(coro, term_lib, 0); lua_setglobal(coro, "term");
    //lua_newtable(coro); luaL_setfuncs(coro, wifi_lib, 0); lua_setglobal(coro, "wifi");
    lua_pop(coro, 2);
    
    lua_pushliteral(coro, "bios.use_multishell=false,shell.autocomplete=false");
    lua_setglobal(coro, "_CC_DEFAULT_SETTINGS");
    lua_pushliteral(coro, "ComputerCraft 1.109.2 (CraftOS-Pi 1.0)");
    lua_setglobal(coro, "_HOST");
    lua_pushnil(coro); lua_setglobal(coro, "package");
    lua_pushnil(coro); lua_setglobal(coro, "require");
    lua_pushnil(coro); lua_setglobal(coro, "io");
    //srand(CTimer::GetClockTicks());
    
    /* Load the file containing the script we are going to run */
    printf("Loading BIOS...\n");
    status = luaL_loadfile(coro, "/rom/bios.lua");
    if (status) {
        /* If something went wrong, error message is at the top of */
        /* the stack */
        const char * fullstr = lua_tostring(coro, -1);
        printf("Couldn't load BIOS: %s (%d)\n", fullstr, status);
        lua_close(L);
        terminal_clear(-1, 0xFE);
        terminal_write_literal(0, 0, "Error loading BIOS", 0xFE);
        terminal_write_string(0, 1, fullstr, 0xFE);
        terminal_write_literal(0, 2, "ComputerCraft may be installed incorrectly", 0xFE);
        halt();
    }
    
    /* Ask Lua to run our little script */
    status = LUA_YIELD;
    int narg = 0;
    printf("Running main coroutine.\n");
    while (status == LUA_YIELD) {
        status = lua_resume(coro, NULL, narg);
        if (status == LUA_YIELD) {
            //printf("Yield\n");
            if (lua_isstring(coro, -1)) narg = getNextEvent(coro, lua_tostring(coro, -1));
            else narg = getNextEvent(coro, NULL);
        } else if (status != 0) {
            const char * fullstr = lua_tostring(coro, -1);
            printf("Errored: %s\n", fullstr);
            lua_close(L);
            terminal_clear(-1, 0xFE);
            terminal_write_literal(0, 0, "Error running computer", 0xFE);
            terminal_write_string(0, 1, fullstr, 0xFE);
            terminal_write_literal(0, 2, "ComputerCraft may be installed incorrectly", 0xFE);
            halt();
        }
    }
    printf("Closing session.\n");
    lua_close(L);
    terminal_clear(-1, 0xFE);
    terminal_write_literal(0, 0, "Error running computer", 0xFE);
    terminal_write_literal(0, 1, "ComputerCraft may be installed incorrectly", 0xFE);
    halt();
}
