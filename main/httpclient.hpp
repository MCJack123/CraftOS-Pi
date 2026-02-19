//
// httpclient.h
//
// Circle - A C++ bare metal environment for Raspberry Pi
// Copyright (C) 2017-2018  R. Stange <rsta2@o2online.de>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
#ifndef _circle_mbedtls_httpclient_h
#define _circle_mbedtls_httpclient_h

#include <circle-mbedtls/http.h>
#include <circle-mbedtls/tlssimplesupport.h>
#include <circle/net/ipaddress.h>
#include <circle/net/netsocket.h>
#include <circle/net/dnsclient.h>
#include <circle/string.h>
#include <circle/types.h>
#include "yuarel.h"

struct THTTPHeaderList {
    char* key;
    char* value;
    THTTPHeaderList * next = NULL;
    THTTPHeaderList(const char* key, size_t klen, const char* value, size_t vlen);
    ~THTTPHeaderList();
};

typedef void (*THTTPHeaderCallback)(void*, const char*, const char*);

class CHTTPClient {
public:
    CHTTPClient(CircleMbedTLS::CTLSSimpleSupport *pTLSSupport, CDNSClient* dns, const char *url);
    ~CHTTPClient(void);

    void SetAutoRedirect(bool redirect);
    void SetTimeout(unsigned int timeout_ms);
    void SetMethod(const char* method);
    void AddHeader(const char* key, const char* value);

    int SendRequest(const char* data = NULL, size_t len = 0);
    size_t GetResponse(const char** buf);
    void GetResponseHeaders(THTTPHeaderCallback cb, void* arg) const;
    int GetResponseCode() const;
    const char* GetResponseMessage() const;

private:
    CircleMbedTLS::CTLSSimpleSupport *m_pTLSSupport;
    CDNSClient *m_pDNSClient;

    struct yuarel m_URL;
    CString m_RequestLine;
    CString m_Headers;
    unsigned int m_Timeout = 10000;
    bool m_ShouldRedirect = false;

    THTTPHeaderList* m_pResponseHeaders = NULL;
    int m_ResponseCode = 0;
    char* m_ResponseMessage = NULL;
    char* m_ResponseData = NULL;
    size_t m_nResponseSize = 0;
};

#endif