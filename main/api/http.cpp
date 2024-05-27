extern "C" {
#include <lua.h>
#include <lauxlib.h>
}
#include <circle/alloc.h>
#include <circle/net/dnsclient.h>
#include <circle/net/ipaddress.h>
#include <circle/util.h>
#include "../event.hpp"
#include "../module/httpclient.hpp"

#define USER_AGENT "computercraft/1.109.2 CraftOS-Pi/1.0"

typedef struct http_header {
    char* key;
    char* value;
    struct http_header* next;
} http_header_t;

struct http_handle_t {
    CHTTPClient handle;
    char* url;
    const char* data = NULL;
    size_t dataLength = 0;
    size_t readPos = 0;
    http_handle_t(CircleMbedTLS::CTLSSimpleSupport *pTLSSupport, CDNSClient* dns, const char *url): handle(pTLSSupport, dns, url) {
        this->url = new char[strlen(url)+1];
        strcpy(this->url, url);
    }
    ~http_handle_t() {
        delete[] url;
    }
};

static bool installed = false;

static int http_handle_read(lua_State *L) {
    http_handle_t** handle = (http_handle_t**)lua_touserdata(L, lua_upvalueindex(1));
    if (*handle == NULL) luaL_error(L, "attempt to use a closed file");
    if ((*handle)->readPos >= (*handle)->dataLength) return 0;
    if (lua_isnumber(L, 1)) {
        int n = lua_tointeger(L, 1);
        if (n < 0) luaL_error(L, "bad argument #1 (value out of range)");
        else if (n == 0) {
            lua_pushliteral(L, "");
            return 1;
        }
        if (n > (*handle)->dataLength - (*handle)->readPos) n = (*handle)->dataLength - (*handle)->readPos;
        lua_pushlstring(L, (*handle)->data + (*handle)->readPos, n);
        (*handle)->readPos += n;
        return 1;
    } else {
        lua_pushinteger(L, (*handle)->data[(*handle)->readPos++]);
        return 1;
    }
}

static int http_handle_readLine(lua_State *L) {
    http_handle_t** handle = (http_handle_t**)lua_touserdata(L, lua_upvalueindex(1));
    if (*handle == NULL) luaL_error(L, "attempt to use a closed file");
    if ((*handle)->readPos >= (*handle)->dataLength) return 0;
    size_t end = (*handle)->readPos;
    while (end < (*handle)->dataLength && (*handle)->data[end] != '\n') end++;
    lua_pushlstring(L, (*handle)->data + (*handle)->readPos, end - (*handle)->readPos);
    (*handle)->readPos = end + 1;
    return 1;
}

static int http_handle_readAll(lua_State *L) {
    http_handle_t** handle = (http_handle_t**)lua_touserdata(L, lua_upvalueindex(1));
    if (*handle == NULL) luaL_error(L, "attempt to use a closed file");
    if ((*handle)->readPos >= (*handle)->dataLength) return 0;
    lua_pushlstring(L, (*handle)->data + (*handle)->readPos, (*handle)->dataLength - (*handle)->readPos);
    (*handle)->readPos = (*handle)->dataLength;
    return 1;
}

static int http_handle_seek(lua_State *L) {
    http_handle_t** handle = (http_handle_t**)lua_touserdata(L, lua_upvalueindex(1));
    if (*handle == NULL) luaL_error(L, "attempt to use a closed file");
    const char* whence = luaL_optstring(L, 1, "cur");
    int offset = luaL_optinteger(L, 2, 0);
    if (strcmp(whence, "set") == 0) {
        if (offset < 0) offset = 0;
        (*handle)->readPos = offset;
    } else if (strcmp(whence, "cur") == 0) {
        if (-offset > (*handle)->readPos) (*handle)->readPos = 0;
        else (*handle)->readPos += offset;
    } else if (strcmp(whence, "end") == 0) {
        if (offset > (*handle)->dataLength) offset = (*handle)->dataLength;
        (*handle)->readPos = (*handle)->dataLength - offset;
    } else luaL_error(L, "Invalid whence");
    lua_pushinteger(L, (*handle)->readPos);
    return 1;
}

static int http_handle_getResponseCode(lua_State *L) {
    http_handle_t** handle = (http_handle_t**)lua_touserdata(L, lua_upvalueindex(1));
    if (*handle == NULL) luaL_error(L, "attempt to use a closed file");
    lua_pushinteger(L, (*handle)->handle.GetResponseCode());
    return 1;
}

static void pushHeader(void* arg, const char* key, const char* value) {
    lua_State *L = (lua_State*)arg;
    lua_pushstring(L, key);
    lua_pushstring(L, value);
    lua_settable(L, -3);
}

static int http_handle_getResponseHeaders(lua_State *L) {
    http_handle_t** handle = (http_handle_t**)lua_touserdata(L, lua_upvalueindex(1));
    if (*handle == NULL) luaL_error(L, "attempt to use a closed file");
    lua_newtable(L);
    (*handle)->handle.GetResponseHeaders(pushHeader, L);
    return 1;
}

static void http_handle_free(http_handle_t* handle) {
    delete handle;
}

static int http_handle_close(lua_State *L) {
    http_handle_t** handle = (http_handle_t**)lua_touserdata(L, lua_upvalueindex(1));
    if (*handle != NULL) http_handle_free(*handle);
    *handle = NULL;
    return 0;
}

static int http_handle_gc(lua_State *L) {
    http_handle_t** handle = (http_handle_t**)lua_touserdata(L, 1);
    if (*handle != NULL) http_handle_free(*handle);
    *handle = NULL;
    return 0;
}

static const luaL_Reg http_handle[] = {
    {"read", http_handle_read},
    {"readLine", http_handle_readLine},
    {"readAll", http_handle_readAll},
    {"seek", http_handle_seek},
    {"getResponseCode", http_handle_getResponseCode},
    {"getResponseHeaders", http_handle_getResponseHeaders},
    {"close", http_handle_close},
    {NULL, NULL}
};

static void http_handle_push(lua_State *L, void* arg) {
    lua_createtable(L, 0, 7);
    http_handle_t** handle_ptr = (http_handle_t**)lua_newuserdata(L, sizeof(http_handle_t*));
    *handle_ptr = (http_handle_t*)arg;
    lua_createtable(L, 0, 1);
    lua_pushcfunction(L, http_handle_gc);
    lua_setfield(L, -2, "__gc");
    lua_setmetatable(L, -2);
    luaL_setfuncs(L, http_handle, 1);
}

class HTTPTask: public CTask {
public:
    HTTPTask(http_handle_t* handle, const char* data, size_t len): m_pHandle(handle), m_pData(data), m_nSize(len) {}

    virtual void Run() override {
        if (m_pHandle->handle.SendRequest(m_pData, m_nSize) < 0) {
            event_t ev;
            ev.type = EVENT_TYPE_HTTP_FAILURE;
            ev.http.url = m_pHandle->url;
            ev.http.err = strerror(errno);
            ev.http.handle_fn = NULL;
            event_push(&ev);
        } else {
            m_pHandle->dataLength = m_pHandle->handle.GetResponse(&m_pHandle->data);
            event_t ev;
            ev.http.url = m_pHandle->url;
            if (m_pHandle->handle.GetResponseCode() >= 400) {
                ev.type = EVENT_TYPE_HTTP_FAILURE;
                ev.http.err = m_pHandle->handle.GetResponseMessage();
            } else {
                ev.type = EVENT_TYPE_HTTP_SUCCESS;
                ev.http.err = NULL;
            }
            ev.http.handle_fn = http_handle_push;
            ev.http.handle_arg = m_pHandle;
            event_push(&ev);
        }
    }

private:
    http_handle_t *m_pHandle;
    const char *m_pData;
    size_t m_nSize;
};

static int http_request(lua_State *L) {
    if (!CNetSubSystem::Get()->IsRunning()) {
        lua_pushboolean(L, false);
        lua_pushliteral(L, "Network not ready");
        return 2;
    }
    if (!lua_istable(L, 1)) {
        lua_settop(L, 4);
        lua_createtable(L, 0, 4);
        lua_pushvalue(L, 1);
        lua_setfield(L, -2, "url");
        lua_pushvalue(L, 2);
        lua_setfield(L, -2, "body");
        lua_pushvalue(L, 3);
        lua_setfield(L, -2, "headers");
        lua_pushvalue(L, 4);
        lua_setfield(L, -2, "binary");
        lua_insert(L, 1);
        lua_settop(L, 1);
    }
    lua_getfield(L, 1, "url");
    http_handle_t * handle = new http_handle_t(CKernel::kernel->TLSSimpleSupport(), CKernel::kernel->DNSClient(), luaL_checkstring(L, -1));
    bool hasMethod = false;
    lua_getfield(L, 1, "method");
    if (lua_isstring(L, -1)) {handle->handle.SetMethod(lua_tostring(L, -1)); hasMethod = true;}
    lua_pop(L, 1);
    lua_getfield(L, 1, "redirect");
    if (lua_isboolean(L, -1)) handle->handle.SetAutoRedirect(lua_toboolean(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "timeout");
    if (lua_isnumber(L, -1)) handle->handle.SetTimeout(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "headers");
    bool ua = false;
    if (lua_istable(L, -1)) {
        lua_pushnil(L);
        while (lua_next(L, -2)) {
            const char* key = lua_tostring(L, -2);
            const char* value = lua_tostring(L, -1);
            handle->handle.AddHeader(key, value);
            if (strcasecmp(key, "User-Agent") == 0) ua = true;
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);
    if (!ua) handle->handle.AddHeader("User-Agent", USER_AGENT);
    lua_getfield(L, 1, "body");
    const char* data = NULL;
    size_t sz = 0;
    if (lua_isstring(L, -1)) {
        data = lua_tolstring(L, -1, &sz); // I sure hope this doesn't get GC'd!!!
        if (!hasMethod) handle->handle.SetMethod("POST");
    }
    lua_pop(L, 1);
    new HTTPTask(handle, data, sz);
    lua_pushboolean(L, true);
    return 1;
}

static void http_checkURL_cb(lua_State *L, void* arg) {
    lua_pushboolean(L, (int)(ptrdiff_t)arg);
}

static int http_checkURL(lua_State *L) {
    size_t sz;
    const char* str = luaL_checklstring(L, 1, &sz);
    ptrdiff_t ok = PTRDIFF_MAX;
    if (strncmp(str, "http://", 7) != 0 && strncmp(str, "https://", 8) != 0) ok = 0;
    event_t event;
    event.type = EVENT_TYPE_HTTP_CHECK;
    event.http.url = (char*)malloc(sz + 1);
    memcpy(event.http.url, str, sz);
    event.http.url[sz] = 0;
    event.http.err = NULL;
    event.http.handle_fn = http_checkURL_cb;
    event.http.handle_arg = (void*)ok;
    event_push(&event);
    lua_pushboolean(L, true);
    return 1;
}

/*struct websocket_message {size_t sz; bool binary; char data[];};

typedef struct {
    esp_websocket_client_handle_t handle;
    char* url;
    bool connected;
    struct websocket_message* partial;
} ws_handle_t;

extern int os_startTimer(lua_State *L);

static int websocket_send(lua_State *L) {
    ws_handle_t* ws = *(ws_handle_t**)lua_touserdata(L, lua_upvalueindex(1));
    if (ws == NULL || !esp_websocket_client_is_connected(ws->handle)) luaL_error(L, "attempt to use a closed file");
    size_t sz;
    const char* data = luaL_checklstring(L, 1, &sz);
    bool binary = lua_toboolean(L, 2);
    (binary ? esp_websocket_client_send_bin : esp_websocket_client_send_text)(ws->handle, data, sz, portMAX_DELAY);
    return 0;
}

static int websocket_receive(lua_State *L) {
    ws_handle_t* ws = *(ws_handle_t**)lua_touserdata(L, lua_upvalueindex(1));
    int tm = 0;
    if (lua_getctx(L, &tm) == LUA_YIELD) {
        if (lua_isstring(L, 1)) {
            if (ws == NULL) {
                lua_pushnil(L);
                return 1;
            }
            const char* ev = lua_tostring(L, 1);
            const char* url = lua_tostring(L, 2);
            if (strcmp(ev, "websocket_message") == 0 && strcmp(url, ws->url) == 0) {
                lua_pushvalue(L, 3);
                lua_pushvalue(L, 4);
                return 2;
            } else if ((strcmp(ev, "websocket_closed") == 0 && strcmp(url, ws->url) == 0 && ws->handle == NULL) ||
                       (tm > 0 && strcmp(ev, "timer") == 0 && lua_isnumber(L, 2) && lua_tointeger(L, 2) == tm)) {
                lua_pushnil(L);
                return 1;
            } else if (strcmp(ev, "terminate") == 0) {
                return luaL_error(L, "Terminated");
            }
        }
    } else {
        if (ws == NULL || !esp_websocket_client_is_connected(ws->handle)) luaL_error(L, "attempt to use a closed file");
        // instead of using native timer routines, we're using os.startTimer so we can be resumed
        if (!lua_isnoneornil(L, 1)) {
            luaL_checknumber(L, 1);
            lua_pushcfunction(L, os_startTimer);
            lua_pushvalue(L, 1);
            lua_call(L, 1, 1);
            tm = lua_tointeger(L, -1);
            lua_pop(L, 1);
        } else tm = -1;
    }
    lua_settop(L, 0);
    return lua_yieldk(L, 0, tm, websocket_receive);
}

static int websocket_close(lua_State *L) {
    ws_handle_t* ws = *(ws_handle_t**)lua_touserdata(L, lua_upvalueindex(1));
    if (ws == NULL) luaL_error(L, "attempt to use a closed file");
    esp_websocket_client_close(ws->handle, portMAX_DELAY);
    esp_websocket_client_destroy(ws->handle);
    if (ws->url) free(ws->url);
    if (ws->partial) free(ws->partial);
    free(ws);
    *(ws_handle_t**)lua_touserdata(L, lua_upvalueindex(1)) = NULL;
    return 0;
}

static int websocket_free(lua_State *L) {
    ws_handle_t* ws = *(ws_handle_t**)lua_touserdata(L, 1);
    if (ws == NULL) return 0;
    esp_websocket_client_close(ws->handle, portMAX_DELAY);
    esp_websocket_client_destroy(ws->handle);
    if (ws->url) free(ws->url);
    if (ws->partial) free(ws->partial);
    free(ws);
    return 0;
}

static const luaL_Reg websocket_funcs[] = {
    {"send", websocket_send},
    {"receive", websocket_receive},
    {"close", websocket_close},
    {NULL, NULL}
};

static void websocket_handle(lua_State *L, void* arg) {
    lua_createtable(L, 0, 3);
    *(ws_handle_t**)lua_newuserdata(L, sizeof(ws_handle_t*)) = arg;
    lua_createtable(L, 0, 1);
    lua_pushcfunction(L, websocket_free);
    lua_setfield(L, -2, "__gc");
    lua_setmetatable(L, -2);
    luaL_setfuncs(L, websocket_funcs, 1);
}

static void websocket_data(lua_State *L, void* arg) {
    struct websocket_message* event = arg;
    lua_pushlstring(L, event->data, event->sz);
    lua_pushboolean(L, event->binary);
    free(event);
}

static void websocket_event_handler(void* handler_arg, esp_event_base_t base, int32_t id, void* data) {
    esp_websocket_event_data_t* event = data;
    ws_handle_t* ws = event->user_context;
    event_t ev;
    switch (id) {
        case WEBSOCKET_EVENT_CONNECTED:
            ws->connected = true;
            ev.type = EVENT_TYPE_WEBSOCKET_SUCCESS;
            ev.http.url = ws->url;
            ev.http.err = NULL;
            ev.http.handle_fn = websocket_handle;
            ev.http.handle_arg = ws;
            event_push(&ev);
            break;
        case WEBSOCKET_EVENT_DATA:
            if ((event->op_code & 0x0F) <= 2) {
                struct websocket_message* message;
                size_t offset = 0;
                if (ws->partial) {
                    offset = ws->partial->sz;
                    message = heap_caps_realloc(ws->partial, sizeof(struct websocket_message) + ws->partial->sz + event->data_len, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
                    ws->partial = NULL;
                } else {
                    message = heap_caps_malloc(sizeof(struct websocket_message) + event->data_len, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
                    message->sz = 0;
                    message->binary = (event->op_code & 0x0F) == 2;
                }
                memcpy(message->data + offset, event->data_ptr, event->data_len);
                message->sz += event->data_len;
                if (message->sz >= event->payload_len) {
                    ev.type = EVENT_TYPE_WEBSOCKET_MESSAGE;
                    ev.http.url = ws->url;
                    ev.http.err = NULL;
                    ev.http.handle_fn = websocket_data;
                    ev.http.handle_arg = message; // race condition?
                    event_push(&ev);
                } else {
                    ws->partial = message;
                }
                break;
            } else if ((event->op_code & 0x0F) == 9) {
                esp_websocket_client_send_with_opcode(ws->handle, 10, (uint8_t*)event->data_ptr, event->data_len, portMAX_DELAY);
                break;
            } else break;
        case WEBSOCKET_EVENT_CLOSED:
        case WEBSOCKET_EVENT_DISCONNECTED:
        case WEBSOCKET_EVENT_ERROR:
            if (!ws->url) break;
            ev.type = ws->connected ? EVENT_TYPE_WEBSOCKET_CLOSED : EVENT_TYPE_WEBSOCKET_FAILURE;
            ev.http.url = ws->url;
            ev.http.err = esp_err_to_name(event->error_handle.esp_tls_last_esp_err);
            ev.http.handle_fn = NULL;
            event_push(&ev);
            ws->url = NULL;
            break;
        default: break;
    }
}

static int http_websocket(lua_State *L) {
    if (lua_istable(L, 1)) {
        lua_settop(L, 1);
        lua_getfield(L, 1, "url");
        lua_getfield(L, 1, "headers");
        lua_getfield(L, 1, "timeout");
        lua_remove(L, 1);
    }
    size_t sz;
    const char* url = luaL_checklstring(L, 1, &sz);
    ws_handle_t* handle = heap_caps_malloc(sizeof(ws_handle_t), MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    if (handle == NULL) {
        lua_pushboolean(L, false);
        return 1;
    }
    esp_websocket_client_config_t config = {
        .uri = url,
        .buffer_size = 131072,
        .use_global_ca_store = true,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .network_timeout_ms = lua_isnumber(L, 3) ? lua_tonumber(L, 3) * 1000 : 5000,
        .disable_auto_reconnect = true,
        .reconnect_timeout_ms = 10000,
        .user_agent = "computercraft/1.109.2 CraftOS-ESP/1.0",
        .task_name = "websocket",
        .task_stack = 4096,
        .task_prio = tskIDLE_PRIORITY,
        .user_context = handle,
    };
    handle->handle = esp_websocket_client_init(&config);
    if (handle->handle == NULL) {
        free(handle);
        lua_pushboolean(L, false);
        return 1;
    }
    handle->url = heap_caps_malloc(sz+1, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    if (handle->url == NULL) {
        esp_websocket_client_destroy(handle->handle);
        free(handle);
        lua_pushboolean(L, false);
        return 1;
    }
    memcpy(handle->url, url, sz);
    handle->url[sz] = 0;
    handle->connected = false;
    handle->partial = NULL;
    if (lua_istable(L, 2)) {
        lua_pushnil(L);
        while (lua_next(L, 2)) {
            const char* key = lua_tostring(L, -2);
            const char* value = lua_tostring(L, -1);
            esp_websocket_client_append_header(handle->handle, key, value);
            lua_pop(L, 1);
        }
    }
    esp_websocket_register_events(handle->handle, WEBSOCKET_EVENT_CONNECTED, websocket_event_handler, NULL);
    esp_websocket_register_events(handle->handle, WEBSOCKET_EVENT_DISCONNECTED, websocket_event_handler, NULL);
    esp_websocket_register_events(handle->handle, WEBSOCKET_EVENT_CLOSED, websocket_event_handler, NULL);
    esp_websocket_register_events(handle->handle, WEBSOCKET_EVENT_DATA, websocket_event_handler, NULL);
    esp_websocket_register_events(handle->handle, WEBSOCKET_EVENT_ERROR, websocket_event_handler, NULL);
    esp_websocket_client_start(handle->handle);
    lua_pushboolean(L, true);
    return 1;
}*/

extern "C" const luaL_Reg http_lib[] = {
    {"request", http_request},
    {"checkURL", http_checkURL},
    //{"websocket", http_websocket},
    {NULL, NULL}
};
