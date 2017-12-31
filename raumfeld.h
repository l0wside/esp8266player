#ifndef RAUMFELD_H
#define RAUMFELD_H

#define STRINGSIZE 500
#define MAXSERVERS 8

#define USE_XML_DESC


typedef struct {
  char *host;
  IPAddress address;
  uint16_t port;
#ifdef USE_XML_DESC
  bool has_volume, has_changevolume, has_playstop, has_prevnext, has_seek, has_getmediainfo, has_gettransportinfo;
#endif
} server_t;

extern server_t servers[];
extern bool server_found;
extern uint8_t n_servers, default_server;


typedef struct {
    const char *action;
    const char *body;
} command_t;

const command_t GETVOLUME = {
    "SOAPACTION: \"urn:schemas-upnp-org:service:RenderingControl:1#GetVolume\"\r\n",
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
    "<s:Envelope s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\" xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\">"
    "<s:Body>"
    "  <u:GetVolume xmlns:u=\"urn:schemas-upnp-org:service:RenderingControl:1\">"
    "     <InstanceID>0</InstanceID>"
    "     <Channel>Master</Channel>" 
    "  </u:GetVolume>"
    "</s:Body>" 
    "</s:Envelope>"
};

const command_t SETVOLUME = {
    "SOAPACTION: \"urn:schemas-upnp-org:service:RenderingControl:1#SetVolume\"\r\n",
    "<s:Envelope s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\" xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\">"
    "<s:Body>"
    "<u:SetVolume xmlns:u=\"urn:schemas-upnp-org:service:RenderingControl:1\">"
    "<InstanceID>0</InstanceID>"
    "<Channel>Master</Channel>"
    "<DesiredVolume>%i</DesiredVolume>"
    "</u:SetVolume>"
    "</s:Body>"
    "</s:Envelope>"
};

    
const command_t PLAY = {
    "SOAPACTION: \"urn:schemas-upnp-org:service:AVTransport:1#Play\"\r\n",
     "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
    "<s:Envelope s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\" xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\">"
    "<s:Body>"
    "<u:Play xmlns:u=\"urn:schemas-upnp-org:service:AVTransport:1\">"
    "<InstanceID>0</InstanceID>"
    "<Speed>1</Speed>"
    "</u:Play>"
    "</s:Body>"
    "</s:Envelope>"
};

const command_t STOP = {
    "SOAPACTION: \"urn:schemas-upnp-org:service:AVTransport:1#Stop\"\r\n",
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
    "<s:Envelope s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\" xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\">"
    "<s:Body>"
    "<u:Stop xmlns:u=\"urn:schemas-upnp-org:service:AVTransport:1\">"
    "<InstanceID>0</InstanceID>"
    "</u:Stop>"
    "</s:Body>"
    "</s:Envelope>"
};

const command_t NEXT = {
    "SOAPACTION: \"urn:schemas-upnp-org:service:AVTransport:1#Next\"\r\n",
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
    "<s:Envelope s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\" xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\">"
    "<s:Body>"
    "<u:Next xmlns:u=\"urn:schemas-upnp-org:service:AVTransport:1\">"
    "<InstanceID>0</InstanceID>"
    "</u:Next>"
    "</s:Body>"
    "</s:Envelope>"
};

const command_t PREV = {
    "SOAPACTION: \"urn:schemas-upnp-org:service:AVTransport:1#Previous\"\r\n",
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
    "<s:Envelope s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\" xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\">"
    "<s:Body>"
    "<u:Previous xmlns:u=\"urn:schemas-upnp-org:service:AVTransport:1\">"
    "<InstanceID>0</InstanceID>"
    "</u:Previous>"
    "</s:Body>"
    "</s:Envelope>"
};

const command_t GETMEDIAINFO = {
    "SOAPACTION: \"urn:schemas-upnp-org:service:AVTransport:1#GetMediaInfo\"\r\n",
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
    "<s:Envelope s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\" xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\">"
    "<s:Body>"
    "<u:GetMediaInfo xmlns:u=\"urn:schemas-upnp-org:service:AVTransport:1\">"
    "<InstanceID>0</InstanceID>"
    "</u:GetMediaInfo>"
    "</s:Body>"
    "</s:Envelope>"
};

const command_t SETMEDIA = {
    "SOAPACTION: \"urn:schemas-upnp-org:service:AVTransport:1#SetAVTransportURI\"\r\n",
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
    "<s:Envelope s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\" xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\">"
    "<s:Body>"
    "<u:SetAVTransportURI xmlns:u=\"urn:schemas-upnp-org:service:AVTransport:1\">"
    "<InstanceID>0</InstanceID>"
    "<CurrentURI>%s</CurrentURI>"
    "<CurrentURIMetaData></CurrentURIMetaData>"
    "</u:SetAVTransportURI>"
    "</s:Body>"
    "</s:Envelope>"
};

const command_t GETTRACK = {
    "SOAPACTION: \"urn:schemas-upnp-org:service:AVTransport:1#GetPositionInfo\"\r\n",
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
    "<s:Envelope s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\" xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\">"
    "<s:Body>"
    "<u:GetPositionInfo xmlns:u=\"urn:schemas-upnp-org:service:AVTransport:1\">"
    "<InstanceID>0</InstanceID>"
    "</u:GetPositionInfo>"
    "</s:Body>"
    "</s:Envelope>"
};

const command_t SEEK = {
    "SOAPACTION: \"urn:schemas-upnp-org:service:AVTransport:1#Seek\"\r\n",
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
    "<s:Envelope s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\" xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\">"
    "<s:Body>"
    "<u:Seek xmlns:u=\"urn:schemas-upnp-org:service:AVTransport:1\">"
    "<InstanceID>0</InstanceID>"
    "<Unit>TRACK_NR</Unit>"
    "<Target>%i</Target>"
    "</u:Seek>"
    "</s:Body>"
    "</s:Envelope>"
};

const command_t GETTRANSPORTINFO = {
    "SOAPACTION: \"urn:schemas-upnp-org:service:AVTransport:1#GetTransportInfo\"\r\n",
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
    "<s:Envelope s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\" xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\">"
    "<s:Body>"
    "<u:GetTransportInfo xmlns:u=\"urn:schemas-upnp-org:service:AVTransport:1\">"
    "<InstanceID>0</InstanceID>"
    "</u:GetTransportInfo>"
    "</s:Body>"
    "</s:Envelope>"
};

void setup_raumfeld();
void clearServers();
void findServers();
bool ping(uint8_t);
void prev();
void next();
void play();
void stop();
int getVolume();
void setVolume(uint16_t);
uint16_t gettrack();
void settrack(int16_t);
String geturi();
void seturi(const char*);
bool playing();



#endif
