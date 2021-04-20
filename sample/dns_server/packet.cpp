#include <arpa/inet.h>
#include <string.h>
#include "packet.h"

uint16_t Packet::extract16Bits(const void *p)
{
    return (uint16_t) ntohs(*(const uint16_t*)p);
}

uint32_t Packet::extract32Bits(const void *p)
{
    return (uint32_t) ntohl(*(const uint32_t *)p);
}

void Packet::parseHeader(uint8_t *&buffer)
{
    header = *(struct DnsHeader *)buffer;
    header.qucount = extract16Bits(&header.qucount);     
    header.adcount = extract16Bits(&header.adcount);     
    buffer += sizeof(struct DnsHeader);
}

void Packet::parse(uint8_t *buffer)
{
    uint8_t *p = buffer;
    parseHeader(p);
    parseQname(p);
    qType = get16Bits(p);
    ///printf("========== query qType %u\n", qType);
    qClass = get16Bits(p);
    csubnet = false;
    if (header.adcount) {
        parseAdditional(p);
    }
}

void Packet::parseQname(uint8_t *&buffer)
{
    uint8_t     i;
    uint8_t     labelSize;
    char        c;
    struct Label label = {0, NULL};
    
    qNameStr = "";
    qName = buffer;
    labelSize = *buffer; 
    
    if (!labelSize) {
        return;
    }

    label.len =  labelSize;
    label.addr = buffer++;
    labels.push_back(label);    

    while (qNameStr.length() < 256) {
        for (i = 0; i < labelSize; i++) {
            c = *buffer++;
            qNameStr += c;
        } 
        labelSize = *buffer;
        if (!labelSize) {
            break;
        }
        qNameStr += '.';
        label.len = labelSize;
        label.addr = buffer++;
        labels.push_back(label);
    }
    //printf("==========query qName %s, queryLen %d\n", qNameStr.c_str(), queryLen);
    queryLen = qNameStr.length() + 2 + 4; // +2 +4 ????
}

void Packet::parseAdditional(uint8_t* buffer)
{
    opt = *(struct Optrr *)buffer;
    opt.type = extract16Bits(&opt.type);    
    opt.size = extract16Bits(&opt.size);    
    opt.ttl = extract32Bits(&opt.ttl);
    opt.rdLen = extract16Bits(&opt.rdLen);    
    if (opt.rdLen) {
        parseOption(buffer + 11);
    }
}

void Packet::parseOption(uint8_t *p)
{
    uint16_t    tmp;
    tmp = *(uint16_t *)p;
    switch (tmp) {
        case 0x0800:
        case 0xfA50:
            csubnet = true;
            eo = *(struct Edns0opt *)p;
            eo.code = extract16Bits(&eo.code);
            eo.len = extract16Bits(&eo.len);
            eo.family = extract16Bits(&eo.family);
            if (eo.family == 1) {
                realRemote4 = eo.subAddr;
            }
            if (eo.family == 2) {
                memcpy(realRemote6, &eo.subAddr, 16);
            }
            break; 
        case 0x0A00:
            co = *(struct CookieOpt *)p;
            co.code = extract16Bits(&co.code);
            co.len = extract16Bits(&co.len);
        break;
    }
}

int Packet::pack(char *buf)
{
    char *buffer = buf; 
    char *bufferBegin = buf;
    char *startHeader = buf;
    uint32_t    intip;
    int domainLen;

    domainLen = domainIp.length();
    strncpy(cstr, domainIp.c_str(), domainLen);
    cstr[domainLen] = '\0';
    header.ancount = 0;
    buffer += headerOffset + queryLen;
    ans.rzone = 0xC00C;
    ans.rttl = 120;
    ans.rsize = 4;

    //printf("========== answer qType %u\n", qType);
    char *p = strtok(cstr, ",");
    while (p) {
        put16Bits(buffer, ans.rzone);
        //put16Bits(buffer, qType);
        put16Bits(buffer, 1);
        //put16Bits(buffer, qType);
        put16Bits(buffer, 1);
        put32Bits(buffer, ans.rttl);
        put16Bits(buffer, ans.rsize);
        inet_pton(AF_INET, p, (void *)&intip);
        buffer[0] = (intip & 0xFF);
        buffer[1] = (intip & 0xFF00) >> 8;
        buffer[2] = (intip & 0xFF0000) >> 16;
        buffer[3] = (intip & 0xFF000000) >> 24;
        buffer += 4;
        header.ancount++;
        if (header.ancount >= 10) {
            break;
        }
        p = strtok(NULL, ",");
    }

    if (header.adcount) {
        buffer[0] = opt.name;
        buffer++;
        put16Bits(buffer, opt.type);
        opt.size = 512;
        put16Bits(buffer, opt.size);
        put32Bits(buffer, opt.ttl);
        if (opt.rdLen) {
            opt.rdLen = 12;
        }
        put16Bits(buffer, opt.rdLen);
        
        if (opt.rdLen) {
            if (csubnet) {
                put16Bits(buffer, eo.code);
                put16Bits(buffer, eo.len);
                put16Bits(buffer, eo.family);
                buffer[0] = eo.sourceNetmask;
                buffer[1] = eo.scopeNetmask;
                buffer += 2;
                buffer[0] = eo.subAddr & 0xFF;
                buffer[1] = (eo.subAddr & 0xFF00) >> 8;
                buffer[2] = (eo.subAddr & 0xFF0000) >> 16;
                buffer[3] = (eo.subAddr & 0xFF000000) >> 24;
                buffer += 4;
            } else {
                put16Bits(buffer, co.code);
                put16Bits(buffer, co.len);
                for (unsigned i = 0; i < 8; i++) {
                    buffer[i] = co.clientCookie[i];
                }
                buffer += 8;
            }
        }
    }
    packHeader(startHeader);
    return buffer - bufferBegin;
}

void Packet::packHeader(char *&buffer)
{
    header.qucount = 1;    
    buffer += 2;
    buffer[0] = 0x81;
    buffer[1] = 0x80;
    buffer += 2;
    put16Bits(buffer, header.qucount);
    put16Bits(buffer, header.ancount);
}
