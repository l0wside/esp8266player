#include "credentials.h"
#include "raumfeld.h"

#define BUFSIZE 16384
#define TAGSIZE 30
#define RENDERCONTROL "urn:schemas-upnp-org:service:RenderingControl:1"
#define TRANSPORTCONTROL "urn:schemas-upnp-org:service:AVTransport:1"
#define ACTION_CHANGEVOLUME "ChangeVolume"
#define ACTION_GETVOLUME "GetVolume"
#define ACTION_PLAY "Play"
#define ACTION_SEEK "Seek"
#define ACTION_NEXT "Next"
#define ACTION_GETMEDIAINFO "GetMediaInfo"
#define ACTION_GETTRANSPORTINFO "GetTransportInfo"


server_t servers[MAXSERVERS];
uint8_t n_servers;
uint8_t default_server;
bool server_found;

IPAddress server_ip;


char stringbuf[4*STRINGSIZE];
char http_buf[BUFSIZE];
char _server[STRINGSIZE];
char _path[STRINGSIZE];
char xml_desc[STRINGSIZE];
char scdp_rendering[STRINGSIZE], scdp_transport[STRINGSIZE];

void setup_raumfeld() {
  server_found = false;
  n_servers = 0;

  WiFi.hostByName(SERVER,server_ip);
}

String http_post(const char *server, uint16_t port, const char *path, const char *postdata, const char *extraheader, const char *content_type, uint8_t max_redirects, uint16_t *http_status) {
  uint16_t _port;
  String body;

  bool redirect = true;
  uint8_t n_redirects = 0;

  if (extraheader == 0) {
    extraheader = "";
  }
  if (postdata == NULL) {
    postdata = "";
  }
  if (content_type == 0) {
    content_type = "application/x-www-form-urlencoded";
  }
  if (http_status != 0) {
    *http_status = 0;
  }

  strncpy(_server, server, STRINGSIZE);
  strncpy(_path, path, STRINGSIZE);
  _port = port;

  while ((n_redirects < max_redirects) && redirect) {
    snprintf(http_buf, BUFSIZE, "POST %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "User-Agent: u_http\r\n"
             "Accept: */*\r\n"
             "%s"
             "Content-type: %s\r\n"
             "Content-length: %i\r\n"
             "Connection: close\r\n\r\n"
             "%s\r\n",
             _path,
             _server,
             extraheader,
             content_type,
             strlen(postdata),
             postdata);

    IPAddress server_address;
    delay(0);
    WiFi.hostByName(_server, server_address);
    //    Serial.print(String("Connect to ") + _server + " " + String(_port)+ " ");
    //    Serial.println(server_address);
    delay(0);

    WiFiClient client;
    int res = client.connect(server_address, _port);
    delay(0);
    if (!res) {
      return String();
    }

    //        Serial.print(buf);

    Serial.print("H");
    client.print(http_buf);
    delay(0);
    Serial.print("h");

    unsigned long timeout = millis();
    while (client.available() == 0) {
      delay(0);
      if (millis() - timeout > 5000) {
        Serial.println(">>> Client Timeout !");
        client.stop();
        return String();
      }
    }
    Serial.print("+" + String(client.connected()) + "a:" + String(client.available()) + "~~ ");
    _path[0] = 0;
    delay(0);

    // Read all the lines of the reply from server and print them to Serial
    timeout = millis();
    uint16_t lno = 0;
    bool header = true;
    char *bufptr;
    uint16_t total_len = 0;
    while (client.connected()) {
      delay(0);
      if (millis() - timeout > 5000) {
        Serial.println("POST: Client Timeout 1");
        client.stop();
        return String();
      }

      String line = client.readStringUntil('\n');
      delay(0);
//      Serial.print("["+String(line.length()) +"]");
      if (header) {
//        Serial.println("@" + line);
        line.toCharArray(http_buf, line.length());
        http_buf[line.length()] = 0;
        //        Serial.print("*");
        //        Serial.print(buf);
        //        Serial.println("*");

        if (lno == 0) {
          if ((strlen(http_buf) < 13) || (strncmp("HTTP/1.1 ", http_buf, 9))) {
            /* Not HTTP or no status code */
            Serial.println("Unexpected first header line " + line);
            return String();
          }
          char status[4];
          strncpy(status, http_buf + 9, 3);
          status[3] = 0;
          if (status[0] == '3') {
            redirect = true;
          } else {
            redirect = false;
          }
          Serial.print("HTTP Status " + String(status));
          if (http_status) {
            *http_status = atoi(status);
          }
        }
        if (redirect && (lno > 0) && (!strncmp("LOCATION: ", http_buf, 10) || !strncmp("Location: ", http_buf, 10))) {
          char *addr = http_buf + 10;
          Serial.println("HTTP Redirect to " + String(addr));
          strncpy(_path, addr, STRINGSIZE);
          n_redirects++;
          continue;
        }
/*        if (line.startsWith("Content-Length: 0")) {
          client.stop();
          return String("");
        } */
        if (line.length() < 2) {
          if (redirect && (strlen(_path) == 0)) {
            /* Redirect, end of header, no location */
            client.stop();
            return String();    
          }
          header = false;
          body = "";
        }
      } else { /* Body */
        delay(0);
        if (line.length() > 0) {
          body = body + line;
        }
      }

      lno++;
    }

    client.stop();
  }

  Serial.println("$$" + String(body.length()));
  return body;
}

bool http_get(const char *server, uint16_t port, const char *path, char *outbuf) {

  snprintf(http_buf, BUFSIZE, "GET %s HTTP/1.1\r\n"
           "Host: %s\r\n"
           "User-Agent: u_http\r\n"
           "Accept: */*\r\n"
           "Connection: close\r\n\r\n",
           path,
           server);

  Serial.println("GET: " + String(http_buf));
           
  IPAddress server_address;
  delay(0);
  WiFi.hostByName(server, server_address);
//Serial.print(String("Connect to ") + server + " " + String(port)+ " ");
//Serial.println(server_address);
  delay(0);

  WiFiClient client;
  int res = client.connect(server_address, port);
  delay(0);
  if (!res) {
    Serial.println("GET: Cannot connect to " + String(server) + ", port " + String(port));
    return false;
  }

  //        Serial.print(buf);

//  Serial.print(":H");
  client.print(http_buf);
  delay(0);
//  Serial.print(":h");

  unsigned long timeout = millis();
  while (client.available() == 0) {
    delay(0);
    if (millis() - timeout > 5000) {
      Serial.println(">>> Client Timeout 1");
      client.stop();
      return false;
    }
  }
//  Serial.print(":+" + String(client.connected()) + "a:" + String(client.available()));

  timeout = millis();
  uint16_t lno = 0;
  bool header = true;
  char *bufptr;
  uint16_t total_len = 0;
  String line;
  uint16_t body_pos;
  while (client.connected()) {
    delay(0);
    if (millis() - timeout > 5000) {
      Serial.println("GET: Client Timeout 2");
      client.stop();
      return false;
    }

    delay(0);
    if (header) {
      line = client.readStringUntil('\n');
//    Serial.print("["+String(line.length()) +"]");
      line.toCharArray(http_buf, line.length());
      http_buf[line.length()] = 0;
      if (lno == 0) {
        if ((strlen(http_buf) < 13) || (strncmp("HTTP/1.1 ", http_buf, 9))) {
          /* Not HTTP or no status code */
          Serial.println("Unexpected first header line " + line);
          client.stop();
          return String();
        }
        char status[4];
        strncpy(status, http_buf + 9, 3);
        status[3] = 0;
        if (status[0] != '2') {
          Serial.println("Unhandled status " + String(status));
          client.stop();
          return false;
        }
      }
      if (line.length() < 2) {
        header = false;
        body_pos = 0;
        memset(outbuf,0,BUFSIZE);
//        Serial.println("--");
      }
      lno++;
    } else { /* Body */
      uint16_t len;
      len = client.available();
      if (len > 512) {
        len = 512;
      }
      if (len + body_pos > BUFSIZE) {
        len = BUFSIZE-body_pos - 1;
      }
      client.readBytes(outbuf+body_pos,len);
//      Serial.print("B"+String(body_pos)+":"+String(len)+" ");
      body_pos += len;
      if (body_pos >= BUFSIZE-1) {
        client.stop();
        outbuf[BUFSIZE-1] = 0;
        return true;
      }
    }
  }

  client.stop();
  outbuf[body_pos] = 0;
  Serial.println(":$$" + String(body_pos));
  
  return true;
}

bool findXML(const char *xml,const char *tag,uint16_t *start, uint16_t *end) {
  char tag_start1[TAGSIZE], tag_start2[TAGSIZE], tag_end[TAGSIZE];
  uint16_t m, n;

  uint16_t tag_len = strlen(tag);
  snprintf(tag_start1,TAGSIZE,"<%s>",tag);
  snprintf(tag_start2,TAGSIZE,"<%s ",tag);
  snprintf(tag_end,TAGSIZE,"</%s>",tag);
  
  if (*start + strlen(tag) +  2 >= strlen(xml)) {
    return false;
  }
  for (uint16_t n = *start; n < *end - tag_len - 2; n++) {
    if (!strncmp(xml+n, tag_start1, tag_len+2) || !strncmp(xml+n, tag_start2, tag_len+2)) {
      for (uint16_t m = n; m < *end - tag_len - 3; m++) {
        if (!strncmp(xml+m,tag_end,tag_len+3)) {
          *start = n + tag_len + 2;
          *end = m;
          return true;
        }
      }
    }
  }
  return false;
}

/* Raumfeld functions */

void prev() {
  for (uint8_t srv = 0; srv < n_servers; srv++) {
#ifdef USE_XML_DESC    
    if (servers[srv].has_getmediainfo) {
//    if (servers[srv].has_prevnext) {
#endif
      http_post(servers[srv].host, servers[srv].port, "/TransportService/Control", PREV.body, PREV.action, NULL, 3, NULL);
#ifdef USE_XML_DESC      
      break;
    }
#endif    
  }
}

void next() {
  for (uint8_t srv = 0; srv < n_servers; srv++) {
#ifdef USE_XML_DESC      
    if (servers[srv].has_getmediainfo) {
//    if (servers[srv].has_prevnext) {
#endif    
      http_post(servers[srv].host, servers[srv].port, "/TransportService/Control", NEXT.body, NEXT.action, NULL, 3, NULL);
#ifdef USE_XML_DESC      
    }
#endif    
  }
}

void play() {
  for (uint8_t srv = 0; srv < n_servers; srv++) {
#ifdef USE_XML_DESC      
    if (servers[srv].has_getmediainfo) {
//    if (servers[srv].has_playstop) {
#endif    
      http_post(servers[srv].host, servers[srv].port, "/TransportService/Control", PLAY.body, PLAY.action, NULL, 3, NULL);
#ifdef USE_XML_DESC      
    }
#endif    
  }
}

void stop() {
  for (uint8_t srv = 0; srv < n_servers; srv++) {
#ifdef USE_XML_DESC      
    if (servers[srv].has_getmediainfo) {
//    if (servers[srv].has_playstop) {
#endif    
      http_post(servers[srv].host, servers[srv].port, "/TransportService/Control", STOP.body, STOP.action, NULL, 3, NULL);
#ifdef USE_XML_DESC      
      return;
    }
#endif    
  }
}

int getVolume() {
  for (uint8_t srv = 0; srv < n_servers; srv++) {
#ifdef USE_XML_DESC      
    if (servers[srv].has_getmediainfo) {
//    if (servers[srv].has_volume) {
#endif    
      String data = http_post(servers[srv].host, servers[srv].port, "/RenderingService/Control", GETVOLUME.body, GETVOLUME.action, NULL, 3, NULL);
      if (data.length() < 10) {
        continue;
      }
      for (uint16_t n = 0; n < data.length(); n++) {
        if (data.substring(n).startsWith("<CurrentVolume>")) {
          for (uint16_t m = n; m < data.length(); m++) {
            if (data.substring(m).startsWith("</CurrentVolume>")) {
              return data.substring(n + 15, m).toInt();
            }
          }
        }
      }
    }
#ifdef USE_XML_DESC      
  }  
#endif    
  return -1;
}

void setVolume(uint16_t volume) {
  snprintf(stringbuf,STRINGSIZE,SETVOLUME.body,volume);
  for (uint8_t srv = 0; srv < n_servers; srv++) {
#ifdef USE_XML_DESC      
    if (servers[srv].has_getmediainfo) {
//    if (servers[srv].has_volume) {
#endif    
      Serial.println("setting volume to " + String(volume) + " on port " + String(servers[srv].port));
      http_post(servers[srv].host, servers[srv].port, "/RenderingService/Control", stringbuf, SETVOLUME.action, NULL, 3, NULL);
#ifdef USE_XML_DESC      
      return;
    }
#endif    
  }
}

uint16_t gettrack() {
  for (uint8_t srv = 0; srv < n_servers; srv++) {
#ifdef USE_XML_DESC      
    if (servers[srv].has_getmediainfo) {
//    if (servers[srv].has_gettransportinfo) {
#endif    
      String data = http_post(servers[srv].host, servers[srv].port, "/TransportService/Control", GETTRACK.body, GETTRACK.action, NULL, 3, NULL);
      for (uint16_t n = 0; n < data.length(); n++) {
        if (data.substring(n).startsWith("<Track>")) {
          for (uint16_t m = n; m < data.length(); m++) {
            if (data.substring(m).startsWith("</Track>")) {
              return (uint16_t)(data.substring(n + 7, m).toInt());
            }
          }
        }
      }
#ifdef USE_XML_DESC      
    }
#endif    
  }
  return 0;
}

void settrack(int16_t track) {
  if (track <= 0) {
    return;
  }
  snprintf(stringbuf,STRINGSIZE,SEEK.body,track);
  for (uint8_t srv = 0; srv < n_servers; srv++) {
#ifdef USE_XML_DESC      
    if (servers[srv].has_getmediainfo) {
//    if (servers[srv].has_seek) {
#endif    
      http_post(servers[srv].host, servers[srv].port, "/TransportService/Control", stringbuf, SEEK.action, NULL, 3, NULL);
#ifdef USE_XML_DESC      
      return;
    }
#endif    
  }
}

String geturi() {
  for (uint8_t srv = 0; srv < n_servers; srv++) {
#ifdef USE_XML_DESC      
    if (servers[srv].has_getmediainfo) {
#endif    
      String data = http_post(servers[srv].host, servers[srv].port, "/TransportService/Control", GETMEDIAINFO.body, GETMEDIAINFO.action, NULL, 3, NULL);
      if (data.length() < 10) {
        continue;
      }

      for (uint16_t n = 0; n < data.length(); n++) {
        if (data.substring(n).startsWith("<CurrentURI>")) {
          for (uint16_t m = n; m < data.length(); m++) {
            if (data.substring(m).startsWith("</CurrentURI>")) {
              return data.substring(n + 12, m);
            }
          }
        }
      }
#ifdef USE_XML_DESC      
    }
#endif
  }
  return String("NONE");
}


void seturi(const char *uri) {
  uint16_t status;
  snprintf(stringbuf,STRINGSIZE*2,SETMEDIA.body,uri);
  Serial.println("Setting URI " + String(uri));
  for (uint8_t srv = 0; srv < n_servers; srv++) {
#ifdef USE_XML_DESC      
    if (servers[srv].has_getmediainfo) {
#endif
      http_post(servers[srv].host, servers[srv].port, "/TransportService/Control", stringbuf, SETMEDIA.action, NULL, 3, &status);
      if (status == 200) {
        return;
      }
#ifdef USE_XML_DESC      
      return;
    }
#endif
  }
}

bool playing() {
  for (uint8_t srv = 0; srv < n_servers; srv++) {
#ifdef USE_XML_DESC      
    if (servers[srv].has_getmediainfo) {
//    if (servers[srv].has_gettransportinfo) {
#endif
      String data = http_post(servers[srv].host, servers[srv].port, "/TransportService/Control", GETTRANSPORTINFO.body, GETTRANSPORTINFO.action, NULL, 3, NULL);
      for (uint16_t n = 0; n < data.length(); n++) {
        if (data.substring(n).startsWith("<CurrentTransportState>")) {
          for (uint16_t m = n; m < data.length(); m++) {
            if (data.substring(m).startsWith("</CurrentTransportState>")) {
              if (data.substring(n + 23, m) == String("PLAYING")) {
                return true;
              }
              return false;
            }
          }
        }
      }
    }
#ifdef USE_XML_DESC      
  }
#endif
  return false;
}


void addServer(String type, String location) {
  yield();
  Serial.println("Adding server at location " + location);
  if (String("urn:schemas-upnp-org:device:MediaRenderer:1") != type) {
    return;
  }
  if (!location.startsWith("http://")) {
    return;
  }
  location = location.substring(7); // remove "http://"
  String server, path;
  uint16_t slash = location.indexOf('/');
  if (slash <= 0) {
    server = location;
    path = "/";
  } else {
    server = location.substring(0, slash);
    path = location.substring(slash);
  }
  uint16_t colon = server.indexOf(':');
  uint16_t port;
  if (colon <= 0) {
    port = 80;
  } else {
    port = server.substring(colon + 1).toInt();
    server = server.substring(0, colon);
  }
  Serial.println(String(":::::::") + server);
  servers[n_servers].host = (char*)malloc(server.length() + 2);
  server.toCharArray(servers[n_servers].host, server.length() + 1);
  servers[n_servers].port = port;

  yield();

  WiFi.hostByName(servers[n_servers].host, servers[n_servers].address);
  delay(0);
  /* Use only services on SERVER */
  if (servers[n_servers].address != server_ip) {
    return;
  }
#ifdef USE_XML_DESC
  servers[n_servers].has_volume = false;
  servers[n_servers].has_changevolume = false;
  servers[n_servers].has_playstop = false;
  servers[n_servers].has_seek = false;
  servers[n_servers].has_getmediainfo = false;
  servers[n_servers].has_gettransportinfo = false;
  path.toCharArray(xml_desc,STRINGSIZE);
  Serial.println("Fetching XML description at " + String(servers[n_servers].host) + ":" + String(servers[n_servers].port) + String(xml_desc));
  delay(0);
  if (!http_get(servers[n_servers].host,servers[n_servers].port,xml_desc,http_buf)) {
    Serial.println("...failed");
    return;
  }
//  Serial.println("read " + String(strlen(http_buf)));
  delay(0);
  uint16_t pos = 0;
  uint16_t service_start, service_end;
  bool found_service = true;
  service_start = 0;
  while (found_service) {
    yield();
    Serial.print("&"+String(service_start));
    service_end = strlen(http_buf);
    found_service = findXML(http_buf,"service",&service_start,&service_end);
    if (!found_service) {
      break;
    }
    uint16_t svc_type_start = service_start, svc_type_end = service_end;
    bool found_svc_type = findXML(http_buf,"serviceType",&svc_type_start,&svc_type_end);
    if (found_svc_type && !strncmp(http_buf+svc_type_start,RENDERCONTROL,strlen(RENDERCONTROL))) {
      Serial.println("Found render control");
      memset(scdp_rendering,0,STRINGSIZE);
      uint16_t scdp_start = service_start, scdp_end = service_end;
      if (findXML(http_buf,"SCPDURL",&scdp_start,&scdp_end)) {
        if (http_buf[scdp_start] == '/') {
          strncpy(scdp_rendering,http_buf+scdp_start,scdp_end-scdp_start);
        } else {
          scdp_rendering[0] = '/';
          strncpy(scdp_rendering+1,http_buf+scdp_start,scdp_end-scdp_start);
        }
        Serial.println("Found render action " + String(scdp_rendering));
        Serial.print("SCDP is: ");
        Serial.println(String(scdp_rendering));
      }
    }
    if (found_svc_type && !strncmp(http_buf+svc_type_start,TRANSPORTCONTROL,strlen(TRANSPORTCONTROL))) {
      Serial.println("Found transport control");
      memset(scdp_transport,0,STRINGSIZE);
      uint16_t scdp_start = service_start, scdp_end = service_end;
      if (findXML(http_buf,"SCPDURL",&scdp_start,&scdp_end)) {
        if (http_buf[scdp_start] == '/') {
          strncpy(scdp_transport,http_buf+scdp_start,scdp_end-scdp_start);
        } else {
          scdp_transport[0] = '/';
          strncpy(scdp_transport+1,http_buf+scdp_start,scdp_end-scdp_start);
        }
        Serial.print("SCDP is: ");
        Serial.println(String(scdp_transport));
      }
    }
  }
  yield();
  if (strlen(scdp_rendering) > 0) {
    Serial.println("Fetching Rendering description, path " + String(scdp_rendering));
    yield();
    if (http_get(servers[n_servers].host,servers[n_servers].port,scdp_rendering,http_buf)) {
      uint16_t action_start = 0;
      uint16_t action_end = strlen(http_buf);
      while (findXML(http_buf,"action",&action_start,&action_end)) {
        uint16_t name_start = action_start;
        uint16_t name_end = action_end;
        if (findXML(http_buf,"name",&name_start,&name_end)) {
          if (!strncmp(http_buf+name_start,ACTION_CHANGEVOLUME,name_end-name_start)) {
            servers[n_servers].has_changevolume = true;
          }
          if (!strncmp(http_buf+name_start,ACTION_GETVOLUME,name_end-name_start)) {
            servers[n_servers].has_volume = true;
          }
          memset(scdp_rendering,0,STRINGSIZE);
          strncpy(scdp_rendering,http_buf+name_start,name_end-name_start);
          Serial.println("Found render action " + String(scdp_rendering));
        }
        action_start = action_end;
        action_end = strlen(http_buf);
      }
    }
  }
  if (strlen(scdp_transport) > 0) {
    Serial.println("Fetching Transport description, path " + String(scdp_transport));
    yield();
    if (http_get(servers[n_servers].host,servers[n_servers].port,scdp_transport,http_buf)) {
      uint16_t action_start = 0;
      uint16_t action_end = strlen(http_buf);
      while (findXML(http_buf,"action",&action_start,&action_end)) {
        uint16_t name_start = action_start;
        uint16_t name_end = action_end;
        if (findXML(http_buf,"name",&name_start,&name_end)) {
          if (!strncmp(http_buf+name_start,ACTION_PLAY,name_end-name_start)) {
            servers[n_servers].has_playstop = true;
          }
          if (!strncmp(http_buf+name_start,ACTION_SEEK,name_end-name_start)) {
            servers[n_servers].has_seek = true;
          }
          if (!strncmp(http_buf+name_start,ACTION_NEXT,name_end-name_start)) {
            servers[n_servers].has_prevnext = true;
          }
          if (!strncmp(http_buf+name_start,ACTION_GETMEDIAINFO,name_end-name_start)) {
            servers[n_servers].has_getmediainfo = true;
          }
          if (!strncmp(http_buf+name_start,ACTION_GETTRANSPORTINFO,name_end-name_start)) {
            servers[n_servers].has_gettransportinfo = true;
          }
          memset(scdp_transport,0,STRINGSIZE);
          strncpy(scdp_transport,http_buf+name_start,name_end-name_start);
          Serial.println("Found transport action " + String(scdp_transport));
        }
        action_start = action_end;
        action_end = strlen(http_buf);
      }
    }
  }
  service_start = service_end;
#endif
    
  Serial.println("ADD SERVER DONE, port " + String(servers[n_servers].port));
#ifdef USE_XML_DESC  
  Serial.println("volume: " + String(servers[n_servers].has_volume)
                                 + "change volume: " + String(servers[n_servers].has_changevolume)
                                 + "play/stop: " + String(servers[n_servers].has_playstop)
                                 + "MediaInfo: " + String(servers[n_servers].has_getmediainfo)
                                 + "TransportInfo: " + String(servers[n_servers].has_gettransportinfo));
#endif                                 
  n_servers++;
}

void clearServers() {
  for (uint8_t n = 0; n < n_servers; n++) {
    free(servers[n].host);
    servers[n].host = NULL;
  }
  n_servers = 0;
}

void findServers() {
  Serial.println("findServers()");
  String device_data = http_post(SERVER, 47365, "/listDevices", NULL, NULL, "application/x-www-form-urlencoded", 3, NULL);
  if (device_data.length() < 30) {
    Serial.println("http_post() returned empty string, no servers found");
    return;
  }
  Serial.println("http_post() returned " + String(device_data.length()) + " bytes");
  for (uint16_t n = 0; n < device_data.length() - 30; n++) {
    yield();
    if (device_data.substring(n).startsWith("<device ")) {
      Serial.println("findServers(): index " + String(n));
      String type, location;
      for (uint16_t m = n; m < device_data.length() - 12; m++) {
        if (device_data.substring(m).startsWith("</device>")) {
          n = m;
          addServer(type, location);
          break;
        }
        if (device_data.substring(m).startsWith("type='")) {
          uint16_t endpos = device_data.substring(m + 6).indexOf('\'');
          if (endpos > 0) {
            type = device_data.substring(m + 6, m + endpos + 6);
          }
        }
        if (device_data.substring(m).startsWith("location='")) {
          uint16_t endpos = device_data.substring(m + 10).indexOf('\'');
          if (endpos > 0) {
            location = device_data.substring(m + 10, m + endpos + 10);
          }
        }
      }
    }
  }
  device_data = "";
  Serial.println("Found " + String (n_servers) + " servers");
}

bool ping(uint8_t srv_index) {
  if (srv_index >= n_servers) {
    return false;
  }

  WiFiClient client;
  int res = client.connect(servers[srv_index].address, servers[srv_index].port);
  client.stop();
  if (!res) {
    return false;
  }
  return true;
}
