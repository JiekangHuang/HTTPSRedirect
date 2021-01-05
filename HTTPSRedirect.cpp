/*  HTTPS on ESP8266 with follow redirects, chunked encoding support
 *  Version 2.1
 *  Author: Sujay Phadke
 *  Github: @electronicsguy
 *  Copyright (C) 2017 Sujay Phadke <electronicsguy123@gmail.com>
 *  All rights reserved.
 *
 */

#include "HTTPSRedirect.h"
#include "DebugMacros.h"
#include <Arduino.h>

HTTPSRedirect::HTTPSRedirect(Client& Client) : 
    _Client(& Client), _httpsPort(80) {
  Init();
}

HTTPSRedirect::~HTTPSRedirect(){ 
}

void HTTPSRedirect::Init(void){
  _keepAlive = true;
  _printResponseBody = false;
  _maxRedirects = 10;
  _contentTypeHeader = "application/x-www-form-urlencoded";
}

// This is the main function which is similar to the method
// print() from WifiClient or WifiClientSecure
bool HTTPSRedirect::printRedir(void){
  unsigned int httpStatus;
  
  // Check if connection to host is alive
  if (!_Client->connected()){
    Serial.println("Error! Not connected to host.");
    return false;
  }

  // Clear the input stream of any junk data before making the request
  while(_Client->available())
    _Client->read();
  
  // Create HTTP/1.1 compliant request string
  // HTTP/1.1 complaint request packet must exist
  
  DPRINTLN(_Request);
  
  // Make the actual HTTPS request using the method 
  // print() from the WifiClientSecure class
  // Make sure the input stream is cleared (as above) before making the call
  _Client->print(_Request);

  // Read HTTP Response Status lines
  while (_Client->connected()) {
    
    httpStatus = getResponseStatus();

    // Only some HTTP response codes are checked for
    // http://www.restapitutorial.com/httpstatuscodes.html
    switch (httpStatus){
      // Success. Fetch final response body
      case 200:
      case 201:
        {
          // final header is discarded
          fetchHeader();
          
          #ifdef EXTRA_FNS
          printHeaderFields();
          #endif
    
          if (_hF.transferEncoding == "chunked")
            fetchBodyChunked();
          else
            fetchBodyUnChunked(_hF.contentLength);
          
          return true;
        }
        break;
        
      case 301:
      case 302:
        {
          // Get re-direction URL from the 'Location' field in the header
          if (getLocationURL()){
            //stop(); // may not be required

            _myResponse.redirected = true;
            
            // Make a new connection to the re-direction server
            if (!_Client->connect(_redirHost.c_str(), _httpsPort)) {
              Serial.println("Connection to re-directed URL failed!");
              return false;
            }

            // Recursive call to the requested URL on the server
            return printRedir();

          }
          else{
            Serial.println("Unable to retrieve redirection URL!");
            return false;
            
          }
        }  
        break;
        
      default:
        Serial.print("Error with request. Response status code: ");
        Serial.println(httpStatus);
        return false;
        break;
    } // end of switch

  }  // end of while

  return false;
  
}

// Create a HTTP GET request packet
// GET headers must be terminated with a "\r\n\r\n"
// http://stackoverflow.com/questions/6686261/what-at-the-bare-minimum-is-required-for-an-http-request
void HTTPSRedirect::createGetRequest(const String& url, const char* host){
  _Request =  String("GET ") + url + " HTTP/1.1\r\n" +
                          "Host: " + host + "\r\n" +
                          "User-Agent: ESP8266\r\n" +
                          (_keepAlive ? "" : "Connection: close\r\n") + 
                          "\r\n\r\n";

  return;
}

// Create a HTTP POST request packet
// POST headers must be terminated with a "\r\n\r\n"
// POST requests have 1 single blank like between the end of the header fields and the body payload
void HTTPSRedirect::createPostRequest(const String& url, const char* host, const String& payload){
  // Content-Length is mandatory in POST requests
  // Body content will include payload and a newline character
  unsigned int len = payload.length() + 1;
  
  _Request =  String("POST ") + url + " HTTP/1.1\r\n" +
                          "Host: " + host + "\r\n" +
                          "User-Agent: ESP8266\r\n" +
                          (_keepAlive ? "" : "Connection: close\r\n") +
                          "Content-Type: " + _contentTypeHeader + "\r\n" + 
                          "Content-Length: " + len + "\r\n" +
                          "\r\n" +
                          payload + 
                          "\r\n\r\n";

  return;
}


bool HTTPSRedirect::getLocationURL(void){

  bool flag;

  // Keep reading from the input stream till we get to 
  // the location field in the header
  flag = _Client->find("Location: ");

  if (flag){
    // Skip URI protocol (http, https, etc. till '//')
    // This assumes that the location field will be containing
    // a URL of the form: http<s>://<hostname>/<url>
    _Client->readStringUntil('/');
    _Client->readStringUntil('/');
    // get hostname
    _redirHost = _Client->readStringUntil('/');
    // get remaining url
    _redirUrl = String('/') + _Client->readStringUntil('\n');
  }
  else{
    DPRINT("No valid 'Location' field found in header!");
  }

  // Create a GET request for the new location
  createGetRequest(_redirUrl, _redirHost.c_str());

  DPRINT("_redirHost: ");
  DPRINTLN(_redirHost);
  DPRINT("_redirUrl: ");
  DPRINTLN(_redirUrl);
    
  return flag;
}

void HTTPSRedirect::fetchHeader(void){
  String line = "";
  int pos = -1;
  int pos2 = -1;
  int pos3 = -1;

  _hF.transferEncoding = "";
  _hF.contentLength = 0;
  
  #ifdef EXTRA_FNS  
  _hF.contentType = "";
  #endif
  
  while (_Client->connected()) {
    line = _Client->readStringUntil('\n');
    
    DPRINTLN(line);
    
    // HTTP headers are terminated by a CRLF ('\r\n')
    // Hence the final line will contain only '\r' 
    // since we have already till the end ('\n')
    if (line == "\r")
      break;

    if (pos < 0){
      pos = line.indexOf("Transfer-Encoding: ");
      if (!pos)
        // get string & remove trailing '\r' character to facilitate string comparisons
        _hF.transferEncoding = line.substring(19, line.length()-1);
    }
    if (pos2 < 0){
      pos2 = line.indexOf("Content-Length: ");
      if (!pos2)
        _hF.contentLength = line.substring(16).toInt();
    }
    #ifdef EXTRA_FNS
    if (pos3 < 0){
      pos3 = line.indexOf("Content-Type: ");
      if (!pos3)
        // get string & remove trailing '\r' character to facilitate string comparisons
        _hF.contentType = line.substring(14, line.length()-1);
    }
    #endif
      
  }

  return;
}

void HTTPSRedirect::fetchBodyUnChunked(unsigned len){
  String line;
  DPRINTLN("Body:");

  while ((_Client->connected()) && (len > 0)) {
    line = _Client->readStringUntil('\n');
    len -= line.length();
    // Content length will include all '\n' terminating characters
    // Decrement once more to account for the '\n' line ending character
    --len;

    if (_printResponseBody)
      Serial.println(line);

    _myResponse.body += line;
    _myResponse.body += '\n';

  }
}

// Ref: http://mihai.ibanescu.net/chunked-encoding-and-python-requests
// http://fssnip.net/2t
void HTTPSRedirect::fetchBodyChunked(void){
  String line;
  int chunkSize;

  while (_Client->connected()){
    line = _Client->readStringUntil('\n');

    // Skip any empty lines
    if (line == "\r")
      continue;
      
    // Chunk sizes are in hexadecimal so convert to integer
    chunkSize = (uint32_t) strtol((const char *) line.c_str(), NULL, 16);
    DPRINT("Chunk Size: ");
    DPRINTLN(chunkSize);

    // Terminating chunk is of size 0
    if (chunkSize == 0)
      break;
    
    while (chunkSize > 0){
      line = _Client->readStringUntil('\n');
      if (_printResponseBody)
        Serial.println(line);

      _myResponse.body += line;
      _myResponse.body += '\n';
      
      chunkSize -= line.length();
      // The line above includes the '\r' character 
      // which is not part of chunk size, so account for it
      --chunkSize;
    }
    
    // Skip over chunk trailer
    
  }
  
  return;

}

unsigned int HTTPSRedirect::getResponseStatus(void){
  // Read response status line
  // ref: https://www.tutorialspoint.com/http/http_responses.htm

  unsigned int statusCode;
  String reasonPhrase;
  String line;

  unsigned int pos = -1;
  unsigned int pos2 = -1;

  // Skip any empty lines
  do{
    line = _Client->readStringUntil('\n');
  }while(line.length() == 0);
  
  pos = line.indexOf("HTTP/1.1 ");
  pos2 = line.indexOf(" ", 9);
  
  if (!pos){
    statusCode = line.substring(9, pos2).toInt();
    reasonPhrase = line.substring(pos2+1, line.length()-1);
  }
  else{
    DPRINTLN("Error! No valid Status Code found in HTTP Response.");
    statusCode = 0;
    reasonPhrase = "";
  }
  
  _myResponse.statusCode = statusCode;
  _myResponse.reasonPhrase = reasonPhrase;
  
  DPRINT("Status code: ");
  DPRINTLN(statusCode);
  DPRINT("Reason phrase: ");
  DPRINTLN(reasonPhrase);

  return statusCode;
}

bool HTTPSRedirect::connect(const String& host, int port) {
    _redirHost = host;
    _httpsPort = port;
    return _Client->connect(host.c_str(), port);
}

void HTTPSRedirect::stop(void) {
    _Client->stop();
}

bool HTTPSRedirect::GET(const String& url){
  return GET(url, _printResponseBody);
}

bool HTTPSRedirect::GET(const String& url, const bool& disp){
  bool retval;
  bool oldval;

  // set _printResponseBody temporarily to argument passed
  oldval = _printResponseBody;
  _printResponseBody = disp;

  // redirected Host and Url need to be initialized in case a 
  // reConnectFinalEndpoint() request is made after an initial request 
  // which did not have redirection
  _redirUrl = url;
  
  InitResponse();
  
  // Create request packet
  createGetRequest(url, _redirHost.c_str());

  // Calll request handler
  retval = printRedir();

  _printResponseBody = oldval;
  return retval;
}

bool HTTPSRedirect::POST(const String& url, const String& payload){
  return POST(url, payload, _printResponseBody);
}

bool HTTPSRedirect::POST(const String& url, const String& payload, const bool& disp){
  bool retval;
  bool oldval;

  // set _printResponseBody temporarily to argument passed
  oldval = _printResponseBody;
  _printResponseBody = disp;

  // redirected Host and Url need to be initialized in case a 
  // reConnectFinalEndpoint() request is made after an initial request 
  // which did not have redirection
  _redirUrl = url;
  
  InitResponse();
  
  // Create request packet
  createPostRequest(url, _redirHost.c_str(), payload);

  // Call request handler
  retval = printRedir();

  _printResponseBody = oldval;
  return retval;
}

void HTTPSRedirect::InitResponse(void){
  // Init response data
  _myResponse.body = "";
  _myResponse.statusCode = 0;
  _myResponse.reasonPhrase = "";
  _myResponse.redirected = false;
}

int HTTPSRedirect::getStatusCode(void){
  return _myResponse.statusCode;
}

String HTTPSRedirect::getReasonPhrase(void){
  return _myResponse.reasonPhrase;
}

String HTTPSRedirect::getResponseBody(void){
  return _myResponse.body;
}

void HTTPSRedirect::setPrintResponseBody(bool disp){
  _printResponseBody = disp;
}

void HTTPSRedirect::setMaxRedirects(const unsigned int n){
  _maxRedirects = n;  // to-do: use this in code above
}

void HTTPSRedirect::setContentTypeHeader(const char *type){
  _contentTypeHeader = type;
}

#ifdef OPTIMIZE_SPEED
bool HTTPSRedirect::reConnectFinalEndpoint(void){
  // disconnect if connection already exists
  if (_Client->connected())
    _Client->stop();

  DPRINT("_redirHost: ");
  DPRINTLN(_redirHost);
  DPRINT("_redirUrl: ");
  DPRINTLN(_redirUrl);

  // Connect to stored final endpoint
  if (!_Client->connect(_redirHost.c_str(), _httpsPort)) {
    DPRINTLN("Connection to final URL failed!");
    return false;
  }

  // Valid request packed must already be
  // present at this point in the member variable _Request 
  // from the previous GET() or POST() request
  
  // Make call to final endpoint
  return printRedir();  
}
#endif

#ifdef EXTRA_FNS
void HTTPSRedirect::fetchBodyRaw(void){
  String line;

  while (_Client->connected()){
    line = readStringUntil('\n');
    if (_printResponseBody)
      Serial.println(line);

    _myResponse.body += line;
    _myResponse.body += '\n';
  }
}

void HTTPSRedirect::printHeaderFields(void){
  DPRINT("Transfer Encoding: ");
  DPRINTLN(_hF.transferEncoding);
  DPRINT("Content Length: ");
  DPRINTLN(_hF.contentLength);
  DPRINT("Content Type: ");
  DPRINTLN(_hF.contentType);
}
#endif

