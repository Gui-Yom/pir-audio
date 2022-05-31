#include <TimeLib.h>
#include <NativeEthernet.h>

namespace ntp {

    EthernetUDP Udp;

    IPAddress timeServer(151, 80, 211, 8);
    const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
    byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets
    const int timeZone = 0;

    void initNtp() {
        Udp.begin(8889);
    }

// send an NTP request to the time server at the given address
    void sendNTPpacket(IPAddress &address)
    {
        // set all bytes in the buffer to 0
        memset(packetBuffer, 0, NTP_PACKET_SIZE);
        // Initialize values needed to form NTP request
        // (see URL above for details on the packets)
        packetBuffer[0] = 0b11100011;   // LI, Version, Mode
        packetBuffer[1] = 0;     // Stratum, or type of clock
        packetBuffer[2] = 6;     // Polling Interval
        packetBuffer[3] = 0xEC;  // Peer Clock Precision
        // 8 bytes of zero for Root Delay & Root Dispersion
        packetBuffer[12] = 49;
        packetBuffer[13] = 0x4E;
        packetBuffer[14] = 49;
        packetBuffer[15] = 52;
        // all NTP fields have been given values, now
        // you can send a packet requesting a timestamp:
        Udp.beginPacket(address, 123); //NTP requests are to port 123
        Udp.write(packetBuffer, NTP_PACKET_SIZE);
        Udp.endPacket();
    }

    time_t getNtpTime()
    {
        while (Udp.parsePacket() > 0); // discard any previously received packets
        sendNTPpacket(timeServer);
        uint32_t beginWait = millis();
        while (millis() - beginWait < 1500) {
            int size = Udp.parsePacket();
            if (size >= NTP_PACKET_SIZE) {
                Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
                unsigned long secsSince1900;
                // convert four bytes starting at location 40 to a long integer
                secsSince1900 = (unsigned long) packetBuffer[40] << 24;
                secsSince1900 |= (unsigned long) packetBuffer[41] << 16;
                secsSince1900 |= (unsigned long) packetBuffer[42] << 8;
                secsSince1900 |= (unsigned long) packetBuffer[43];
                return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
            }
        }
        return 0; // return 0 if unable to get the time
    }
}
