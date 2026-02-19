//
// httpclient.cpp
//
// Circle - A C++ bare metal environment for Raspberry Pi
// Copyright (C) 2017-2019  R. Stange <rsta2@o2online.de>
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
#include "httpclient.hpp"
#include <circle-mbedtls/tlssimpleclientsocket.h>
#include <circle/netdevice.h>
#include <circle/util.h>
#include <circle/net/socket.h>
#include <circle/net/in.h>
#include <circle/sched/scheduler.h>
#include <circle/logger.h>
#include <assert.h>
#include <errno.h>

using namespace CircleMbedTLS;

THTTPHeaderList::THTTPHeaderList(const char* _key, size_t klen, const char* _value, size_t vlen) {
    key = new char[klen + 1];
    value = new char[vlen + 1];
    memcpy(key, _key, klen);
    memcpy(value, _value, vlen);
    key[klen] = 0;
    value[vlen] = 0;
}

THTTPHeaderList::~THTTPHeaderList() {
    delete[] key;
    delete[] value;
    if (next != NULL) delete next;
}

CHTTPClient::CHTTPClient(CTLSSimpleSupport *pTLSSupport, CDNSClient *dns, const char *url): m_pDNSClient(dns), m_pTLSSupport(pTLSSupport) {
    char* newurl = new char[strlen(url) + 1];
    strcpy(newurl, url);
    yuarel_parse(&m_URL, newurl);
    if (m_URL.path == NULL) m_URL.path = "";
    if (m_URL.query != NULL) m_URL.query[-1] = '?';
    if (m_URL.port == 0) {
        if (strcasecmp(m_URL.scheme, "https") == 0) m_URL.port = 443;
        else m_URL.port = 80;
    }
    m_RequestLine.Format("GET /%s HTTP/1.1\r\n", m_URL.path);
    m_Headers.Format("Host: %s\r\nConnection: close\r\n", m_URL.host);
}

CHTTPClient::~CHTTPClient(void) {
    if (m_pResponseHeaders != NULL) delete m_pResponseHeaders;
    if (m_ResponseMessage != NULL) delete[] m_ResponseMessage;
    if (m_ResponseData != NULL) free(m_ResponseData);
    delete[] m_URL.scheme;
}

void CHTTPClient::SetAutoRedirect(bool redirect) {
    m_ShouldRedirect = redirect;
}

void CHTTPClient::SetTimeout(unsigned int timeout_ms) {
    m_Timeout = timeout_ms;
}

void CHTTPClient::SetMethod(const char* method) {
    m_RequestLine.Format("%s /%s HTTP/1.1\r\n", method, m_URL.path);
}

void CHTTPClient::AddHeader(const char* key, const char* value) {
    m_Headers.Append(key);
    m_Headers.Append(": ");
    m_Headers.Append(value);
    m_Headers.Append("\r\n");
}

int CHTTPClient::SendRequest(const char* data, size_t len) {
    CLogger::Get()->Write("httpclient", LogNotice, "Connecting to %s://%s/%s...", m_URL.scheme, m_URL.host, m_URL.path);
    CIPAddress ip;
    if (!m_pDNSClient->Resolve(m_URL.host, &ip)) {
        errno = EHOSTUNREACH;
        return -1;
    }
    const u8* addr = ip.Get();
    CLogger::Get()->Write("httpclient", LogDebug, "Resolved URL %s to %d.%d.%d.%d", m_URL.host, addr[0], addr[1], addr[2], addr[3]);

    CNetSocket *m_pSocket;
    if (strcasecmp(m_URL.scheme, "http") == 0) {
        m_pSocket = new CSocket(m_pTLSSupport->GetNet(), IPPROTO_TCP);
        if (m_pSocket == NULL) {
            errno = EIO;
            return -1;
        }
    } else if (strcasecmp(m_URL.scheme, "https") == 0) {
        CTLSSimpleClientSocket *pSSLSocket = new CTLSSimpleClientSocket(m_pTLSSupport, IPPROTO_TCP);
        if (pSSLSocket == NULL) {
            errno = EIO;
            return -1;
        }

        m_pSocket = pSSLSocket;

        if (pSSLSocket->AddCertificatePath("/rom/certs") < 0) {
            delete m_pSocket;
            errno = ENOENT;
            return -1;
        }

        if (pSSLSocket->Setup(m_URL.host) != 0) {
            delete m_pSocket;
            errno = EIO;
            return -1;
        }
    } else {
        errno = EPROTONOSUPPORT;
        return -1;
    }

    if (m_pSocket->Connect(ip, m_URL.port) < 0) {
        delete m_pSocket;
        errno = ECONNREFUSED;
        return -1;
    }

    CString request;
    request.Append(m_RequestLine);
    request.Append(m_Headers);
    if (len > 0) {
        CString h;
        h.Format("Content-Length: %d\r\n", len);
        request.Append(h);
    }
    request.Append("\r\n");
    size_t reqlen = request.GetLength();
    char* reqdata = new char[reqlen + len];
    memcpy(reqdata, request, reqlen);
    if (len > 0) memcpy(reqdata + reqlen, data, len);

    if (m_pSocket->Send(reqdata, reqlen + len, MSG_DONTWAIT) < 0) {
        delete m_pSocket;
        errno = ECONNABORTED;
        return -1;
    }

    char buffer[4096];
    char* bufstart = buffer;
    size_t bufsize = 4096;
    char* resptr = NULL;
    unsigned int starttime = CTimer::Get()->GetTicks();
    while (resptr == NULL || resptr < m_ResponseData + m_nResponseSize) {
        int sz = m_pSocket->Receive(bufstart, bufsize, MSG_DONTWAIT);
        bufstart = buffer;
        bufsize = 4096;
        if (sz == 0) {
            if (m_Timeout > 0 && CTimer::Get()->GetTicks() - starttime > MSEC2HZ(m_Timeout)) {
                delete m_pSocket;
                errno = ETIMEDOUT;
                return -1;
            }
            CScheduler::Get()->Yield();
        } else if (sz < 0) break;
        else {
            char* const bufend = buffer + sz;
            if (resptr) {
                size_t left = m_ResponseData + m_nResponseSize - resptr;
                memcpy(resptr, buffer, left < sz ? left : sz);
                resptr += sz;
                continue;
            }
            while (true) {
                char* nl = bufstart;
                while (nl < bufend && *nl != '\n') nl++;
                if (nl == bufend) break;
                if (m_ResponseCode == 0) {
                    // response message
                    if (nl - bufstart < 9 || strncmp(bufstart, "HTTP/1.1 ", 9) != 0) {
                        delete m_pSocket;
                        errno = EPROTO;
                        return -1;
                    }
                    m_ResponseCode = atoi(bufstart + 9);
                    m_ResponseMessage = new char[nl - bufstart - 13];
                    memcpy(m_ResponseMessage, bufstart + 13, nl - bufstart - 14);
                    m_ResponseMessage[nl - bufstart - 14] = 0;
                    CLogger::Get()->Write("httpclient", LogDebug, "HTTP/1.1 %d %s", m_ResponseCode, m_ResponseMessage);
                } else if (nl - bufstart > 1) {
                    // header
                    char* sep = bufstart;
                    while (sep < nl && *sep != ':') sep++;
                    THTTPHeaderList *h = new THTTPHeaderList(bufstart, sep - bufstart, sep + 2, nl - sep - 3);
                    CLogger::Get()->Write("httpclient", LogDebug, "'%s': '%s'", h->key, h->value);
                    h->next = m_pResponseHeaders;
                    m_pResponseHeaders = h;
                    if (strcasecmp(h->key, "Content-Length") == 0) {
                        m_nResponseSize = atoi(h->value);
                        CLogger::Get()->Write("httpclient", LogDebug, "Content-Length: %d", m_nResponseSize);
                    }
                } else {
                    // start of data
                    if (m_nResponseSize > 0) {
                        m_ResponseData = resptr = new char[m_nResponseSize];
                        size_t left = buffer + sz - (nl + 1);
                        memcpy(resptr, nl + 1, left < m_nResponseSize ? left : m_nResponseSize);
                        resptr += left;
                    } else resptr = (char*)UINTPTR_MAX; // force outer loop to exit
                    break;
                }
                bufstart = nl + 1;
            }
        }
    }

    delete m_pSocket;
    if (m_ResponseCode == 0) {
        errno = EIO;
        return -1;
    } else if (m_ResponseCode / 100 == 3 && m_ShouldRedirect) {
        THTTPHeaderList *node = m_pResponseHeaders;
        while (node && strcasecmp(node->key, "Location") != 0) node = node->next;
        if (!node) return 0;
        delete[] m_URL.scheme;
        char* newurl = new char[strlen(node->value) + 1];
        strcpy(newurl, node->value);
        yuarel_parse(&m_URL, newurl);
        if (m_URL.path == NULL) m_URL.path = "";
        if (m_URL.query != NULL) m_URL.query[-1] = '?';
        if (m_URL.port == 0) {
            if (strcasecmp(m_URL.scheme, "https") == 0) m_URL.port = 443;
            else m_URL.port = 80;
        }
        // TODO: fix path/host
        delete m_pResponseHeaders;
        if (m_ResponseMessage) delete[] m_ResponseMessage;
        if (m_ResponseData) delete[] m_ResponseData;
        m_pResponseHeaders = NULL;
        m_ResponseData = m_ResponseMessage = NULL;
        m_ResponseCode = m_nResponseSize = 0;
        return SendRequest(data, len);
    }
    return 0;
}

size_t CHTTPClient::GetResponse(const char** buf) {
    *buf = m_ResponseData;
    return m_nResponseSize;
}

void CHTTPClient::GetResponseHeaders(THTTPHeaderCallback cb, void* arg) const {
    THTTPHeaderList* node = m_pResponseHeaders;
    while (node) {
        cb(arg, node->key, node->value);
        node = node->next;
    }
}

int CHTTPClient::GetResponseCode() const {
    return m_ResponseCode;
}

const char* CHTTPClient::GetResponseMessage() const {
    return m_ResponseMessage;
}

