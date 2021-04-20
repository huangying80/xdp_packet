#ifndef _PACKET_H_
#define _PACKET_H_
#include <stdint.h>
#include <string>
#include <vector>

using namespace std;
class Packet {
private:
    struct DnsHeader {
        uint16_t        id;
        uint8_t         flags1;
        uint8_t         flags2;
        uint16_t        qucount; //question count
        uint16_t        ancount; //anwser count
        uint16_t        aucount; //authority count
        uint16_t        adcount; // additional count
    } header;

    struct Answer {
        int16_t         rzone;
        int16_t         rtype;
        int16_t         rclass;
        uint32_t        rttl;
        int16_t         rsize;
        string          rdata;
    } ans;

    struct Edns0opt {
        uint16_t        code;
        uint16_t        len;
        uint16_t        family;
        uint8_t         sourceNetmask;
        uint8_t         scopeNetmask;
        uint32_t        subAddr;
    } eo;

    struct CookieOpt {
        uint16_t        code;
        uint16_t        len;
        char            clientCookie[8];
    } co;
    struct Optrr {
        uint8_t         name;
        uint16_t        type;
        uint16_t        size;
        uint32_t        ttl;
        uint16_t        rdLen;
    } opt;

    struct Label {
        uint8_t         len;
        uint8_t        *addr;
    };

    bool            csubnet;
    const uint8_t   headerOffset = 12;
    string          qNameStr;
    uint8_t        *qName;
    uint16_t        qType;
    uint16_t        qClass;
    uint16_t        queryLen;
    uint32_t        remote4;
    uint8_t         remote6[16];
    uint32_t        realRemote4;
    uint8_t         realRemote6[16];
    string          domainIp;
    char            cstr[512];
    vector<Label>   labels;

    inline int get16Bits(uint8_t *&buffer) {
        int value = static_cast<unsigned char> (buffer[0]);
        value = value << 8;
        value += static_cast<unsigned char> (buffer[1]);
        buffer += 2;
        return value;
    }

    inline void put16Bits(char *&buffer, uint16_t value) {
        buffer[0] = (value & 0xFF00) >> 8;
        buffer[1] = value & 0xFF;
        buffer += 2;
    }

    inline void put32Bits(char *&buffer, uint32_t value) {
        buffer[0] = (value & 0xFF000000) >> 24;
        buffer[1] = (value & 0xFF0000) >> 16;
        buffer[2] = (value & 0xFF00) >> 8;
        buffer[3] = (value & 0xFF);
        buffer += 4;
    }
    uint16_t    extract16Bits(const void *p);
    uint32_t    extract32Bits(const void *p);
    void parseQname(uint8_t *&buffer);
    void parseHeader(uint8_t *&buffer);        
    void parseOption(uint8_t *buffer);
    void parseAdditional(uint8_t *buffer);

public:
    Packet(void) {
        qNameStr.reserve(256);
        domainIp.reserve(256);
        labels.clear();
        csubnet = false;
    }
    void parse(uint8_t *buffer);
    void setDomainIpGroup(const char *ip) { domainIp = ip;};
    string &getQNameStr(void) {return qNameStr;};
    uint8_t *getQName(void) {return qName;};
    bool isCSubnet(void) {return csubnet;};
    void packHeader(char *&buffer);
    int pack(char *buffer);
};
#endif
