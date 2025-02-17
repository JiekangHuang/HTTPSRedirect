/*  HTTPS on ESP8266 with follow redirects, chunked encoding support
 *  Version 2.1
 *  Author: Sujay Phadke
 *  Github: @electronicsguy
 *  Copyright (C) 2017 Sujay Phadke <electronicsguy123@gmail.com>
 *  All rights reserved.
 *
 */
#pragma once
#include <Client.h>

// Un-comment for extra functionality
//#define EXTRA_FNS
#define OPTIMIZE_SPEED

class HTTPSRedirect {
  private:
    Client* _Client;
    int _httpsPort;
    bool _keepAlive;
    String _redirUrl;
    String _redirHost;
    unsigned int _maxRedirects;  // to-do
    const char* _contentTypeHeader;
    
    struct headerFields{
      String transferEncoding;
      unsigned int contentLength;
      #ifdef EXTRA_FNS
      String contentType;
      #endif
    };

    headerFields _hF;
    
    String _Request;

    struct Response{
      int statusCode;
      String reasonPhrase;
      bool redirected;
      String body;
    };

    Response _myResponse;
    bool _printResponseBody;

    void Init(void);
    bool printRedir(void);    
    void fetchHeader(void);
    bool getLocationURL(void);
    void fetchBodyUnChunked(unsigned);
    void fetchBodyChunked(void);
    unsigned int getResponseStatus(void);
    void InitResponse(void);
    void createGetRequest(const String&, const char*);
    void createPostRequest(const String&, const char*, const String&);
    
#ifdef EXTRA_FNS
    void fetchBodyRaw(void);
    void printHeaderFields(void);
#endif

  public:

    HTTPSRedirect(Client& Client);
    ~HTTPSRedirect();

    bool connect(const String& host, int port);
    void stop(void);
    bool GET(const String&);
    bool GET(const String&, const bool&);
    bool POST(const String&, const String&);
    bool POST(const String&, const String&, const bool&);

    int getStatusCode(void);
    String getReasonPhrase(void);
    String getResponseBody(void);
    
    void setPrintResponseBody(bool);
    void setMaxRedirects(const unsigned int);
    
    void setContentTypeHeader(const char *);
#ifdef OPTIMIZE_SPEED
    bool reConnectFinalEndpoint(void);
#endif

};
