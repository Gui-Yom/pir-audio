#include <TimeLib.h>
#include <NativeEthernet.h>
#include "Ntp.h"

namespace ntp {

    struct NtpPacket
    {
        uint8_t li_vn_mode;      // Eight bits. li, vn, and mode.
        // li.   Two bits.   Leap indicator.
        // vn.   Three bits. Version number of the protocol.
        // mode. Three bits. Client will pick mode 3 for client.

        uint8_t stratum;         // Eight bits. Stratum level of the local clock.
        uint8_t poll;            // Eight bits. Maximum interval between successive messages.
        uint8_t precision;       // Eight bits. Precision of the local clock.

        uint32_t rootDelay;      // 32 bits. Total round trip delay time.
        uint32_t rootDispersion; // 32 bits. Max error aloud from primary clock source.
        uint32_t refId;          // 32 bits. Reference clock identifier.

        uint32_t refTm_s;        // 32 bits. Reference time-stamp seconds.
        uint32_t refTm_f;        // 32 bits. Reference time-stamp fraction of a second.

        uint32_t origTm_s;       // 32 bits. Originate time-stamp seconds.
        uint32_t origTm_f;       // 32 bits. Originate time-stamp fraction of a second.

        uint32_t rxTm_s;         // 32 bits. Received time-stamp seconds.
        uint32_t rxTm_f;         // 32 bits. Received time-stamp fraction of a second.

        uint32_t txTm_s;         // 32 bits and the most important field the client cares about. Transmit time-stamp seconds.
        uint32_t txTm_f;         // 32 bits. Transmit time-stamp fraction of a second.
    };

#define NTP_PACKET_SIZE sizeof(NtpPacket)
#define NTP_PORT 8123
#define NTP_TZ 0
#define NTP_TIMEOUT_MS 1500
#define NTP_SERVER NTP_SERVER_LOCAL

    IPAddress NTP_SERVER_LOCAL(192, 168, 1, 1);
    IPAddress NTP_SERVER_0(151, 80, 211, 8);
    EthernetUDP Udp;
    NtpPacket packet;

    void begin()
    {
        Udp.begin(8889);
    }

// send an NTP request to the time server at the given address
    void sendNtpReq(IPAddress &address)
    {
        memset(&packet, 0, NTP_PACKET_SIZE);
        packet.li_vn_mode = 0b11100011;
        packet.stratum = 0;
        packet.poll = 4;
        packet.precision = 0xFA;
        Udp.beginPacket(address, NTP_PORT);
        Udp.write((char*) &packet, NTP_PACKET_SIZE);
        Udp.endPacket();
    }

    time_t getTime()
    {
        while (Udp.parsePacket() > 0); // discard any previously received packets
        sendNtpReq(NTP_SERVER);
        uint32_t beginWait = millis();
        while (millis() - beginWait < NTP_TIMEOUT_MS) {
            int size = Udp.parsePacket();
            if (size >= NTP_PACKET_SIZE) {
                Serial.println("Got NTP response");
                Udp.read((char*) &packet, NTP_PACKET_SIZE);  // read packet into the buffer
                time_t secsSince1900 = fnet_htonl(packet.txTm_s);
                return secsSince1900 - 2208988800UL + NTP_TZ * SECS_PER_HOUR;
            }
        }
        return 0; // return 0 if unable to get the time
    }
}
