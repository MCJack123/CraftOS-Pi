#include <string.h>
extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}
#include <stdlib.h>
#include <craftos.h>
#include <craftos_fatfs.h>
#include <circle/timer.h>
#include <circle/startup.h>
//#include "driver/wifi.hpp"
#include "event.hpp"
#include "httpclient.hpp"

typedef struct timer {
    int id;
    TKernelTimerHandle timer;
    struct timer* next;
} timer_ll_t;

static int nextTimerID = 0;
static timer_ll_t* timer_ll_head = NULL, *timer_ll_tail = NULL;

static double timestamp() {
    return CTimer::Get()->GetTime() * 1000.0 + (CTimer::Get()->GetClockTicks() / 1000) % 1000;
}

static unsigned long convertPixelValue(unsigned char index, unsigned char r, unsigned char g, unsigned char b) {
    return index;
}

static void timer(TKernelTimerHandle hTimer, void *pParam, void *pContext) {
    int id = (int)(ptrdiff_t)pParam;
    event_t event;
    event.type = EVENT_TYPE_TIMER;
    event.timer.timerID = id;
    event_push(&event);
    timer_ll_t* tm = timer_ll_head, *last = NULL;
    while (tm) {
        if (tm->id == id) {
            if (last) last->next = tm->next;
            if (timer_ll_head == tm) timer_ll_head = tm->next;
            if (timer_ll_tail == tm) timer_ll_tail = last;
            free(tm);
            //CTimer::Get()->CancelKernelTimer(hTimer);
            return;
        }
        last = tm;
        tm = tm->next;
    }
}

static int startTimer(unsigned long time, craftos_machine_t machine) {
    int id = nextTimerID++;
    int ticks = MSEC2HZ(time);
    if (ticks <= 0) {
        event_t event;
        event.type = EVENT_TYPE_TIMER;
        event.timer.timerID = id;
        event_push(&event);
    } else {
        TKernelTimerHandle handle = CTimer::Get()->StartKernelTimer(ticks, timer, (void*)(ptrdiff_t)id);
        timer_ll_t* tm = (timer_ll_t*)malloc(sizeof(timer_ll_t));
        tm->id = id;
        tm->timer = handle;
        tm->next = NULL;
        if (timer_ll_tail) timer_ll_tail->next = tm;
        else timer_ll_head = tm;
        timer_ll_tail = tm;
    }
    return id;
}

static void cancelTimer(int id, craftos_machine_t machine) {
    timer_ll_t* tm = timer_ll_head, *last = NULL;
    while (tm) {
        if (tm->id == id) {
            CTimer::Get()->CancelKernelTimer(tm->timer);
            if (last) last->next = tm->next;
            if (timer_ll_head == tm) timer_ll_head = tm->next;
            if (timer_ll_tail == tm) timer_ll_tail = last;
            free(tm);
            return;
        }
        last = tm;
        tm = tm->next;
    }
}

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
            ev.http.handle_arg = m_pHandle;
            event_push(&ev);
        }
    }

private:
    http_handle_t *m_pHandle;
    const char *m_pData;
    size_t m_nSize;
};

static int http_request(const char * url, const char * method, const unsigned char * body, size_t body_size, craftos_http_header_t * headers, int redirect, craftos_machine_t machine) {
    if (!CNetSubSystem::Get()->IsRunning()) {
        return -1;
    }
    http_handle_t * handle = new http_handle_t(CKernel::kernel->TLSSimpleSupport(), CKernel::kernel->DNSClient(), url);
    handle->handle.SetMethod(method);
    handle->handle.SetAutoRedirect(redirect);
    for (int i = 0; headers[i].key; i++) {
        handle->handle.AddHeader(headers[i].key, headers[i].value);
    }
    new HTTPTask(handle, (const char*)body, body_size);
    return 0;
}

static int http_handle_close(craftos_http_handle_t handle, craftos_machine_t machine) {
    delete (http_handle_t*)handle;
    return 0;
}

static size_t http_handle_read(void * buf, size_t size, size_t count, craftos_http_handle_t _handle, craftos_machine_t machine) {
    http_handle_t * handle = (http_handle_t*)_handle;
    size_t n = size * count;
    if (n > handle->dataLength - handle->readPos) n = handle->dataLength - handle->readPos;
    memcpy(buf, handle->data + handle->readPos, n);
    handle->readPos += n;
    return n / size;
}

static int http_handle_getc(craftos_http_handle_t _handle, craftos_machine_t machine) {
    http_handle_t * handle = (http_handle_t*)_handle;
    if (handle->readPos >= handle->dataLength) return EOF;
    return handle->data[handle->readPos++];
}

static long http_handle_tell(craftos_http_handle_t _handle, craftos_machine_t machine) {
    http_handle_t * handle = (http_handle_t*)_handle;
    return handle->readPos;
}

static int http_handle_seek(craftos_http_handle_t _handle, long offset, int origin, craftos_machine_t machine) {
    http_handle_t * handle = (http_handle_t*)_handle;
    switch (origin) {
        case SEEK_SET:
            if (offset < 0 || offset >= handle->dataLength) return -1;
            handle->readPos = offset;
            break;
        case SEEK_CUR:
            if (-offset > handle->readPos || offset >= handle->dataLength - handle->readPos) return -1;
            handle->readPos += offset;
            break;
        case SEEK_END:
            if (offset > 0 || -offset >= handle->dataLength) return -1;
            handle->readPos = handle->dataLength - offset;
            break;
    }
    return 0;
}

static int http_handle_eof(craftos_http_handle_t _handle, craftos_machine_t machine) {
    http_handle_t * handle = (http_handle_t*)_handle;
    return handle->readPos >= handle->dataLength;
}

static int http_handle_getResponseCode(craftos_http_handle_t _handle, craftos_machine_t machine) {
    http_handle_t * handle = (http_handle_t*)_handle;
    return handle->handle.GetResponseCode();
}

static void header_cb(void* arg, const char* key, const char* value) {
    craftos_http_header_t ** header = (craftos_http_header_t**)arg;
    if (*header == NULL) {
        *header = new craftos_http_header_t {key, value};
    } else if (strcmp((*header)->key, key) == 0) {
        delete *header;
        *header = NULL;
    }
}

static void http_handle_getResponseHeader(craftos_http_handle_t _handle, craftos_http_header_t ** header, craftos_machine_t machine) {
    http_handle_t * handle = (http_handle_t*)_handle;
    handle->handle.GetResponseHeaders(header_cb, header);
}

extern const craftos_func_t funcs = {
    timestamp,
    convertPixelValue,
    startTimer,
    cancelTimer,
    NULL,
    NULL, NULL, 
    NULL, NULL, NULL, 
    NULL, NULL, NULL, NULL, 
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
    craftos_fatfs_mkdir, craftos_fatfs_access, 
    NULL, 
    craftos_fatfs_statvfs, 
    craftos_fatfs_opendir,
    craftos_fatfs_closedir,
    craftos_fatfs_readdir,
    http_request,
    http_handle_close,
    http_handle_read,
    http_handle_getc,
    http_handle_tell,
    http_handle_seek,
    http_handle_eof,
    http_handle_getResponseCode,
    http_handle_getResponseHeader,
    NULL, NULL, NULL
};
