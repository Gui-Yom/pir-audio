#include <NativeEthernet.h>
#include <TimeLib.h>
#include "Ntp.h"
#include "PacketHeader.h"

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xE0 };
IPAddress ip(192, 168, 1, 2);
//IPAddress gateway(192, 168, 1, 1);

unsigned int localPort = 8888;

// buffers for receiving and sending data
char packetBuffer[UDP_TX_PACKET_MAX_SIZE];

// An EthernetUDP instance to let us send and receive packets over UDP
EthernetUDP Udp;

//#define CONF_DHCP
#define CONF_MANUAL

#define WAIT_FOR_SERIAL

void blockErr()
{
    while (true) {
        delay(1);
    }
}

void setup()
{
#ifdef CONF_DHCP
    bool dhcpFailed = false;
    // start the Ethernet
    if (!Ethernet.begin(mac)) {
        dhcpFailed = true;
    }
#endif
#ifdef CONF_MANUAL
    Ethernet.begin(mac, ip);
#endif

    // Open serial communications
    Serial.begin(9600);

#ifdef WAIT_FOR_SERIAL
    while (!Serial) { }
#endif

    // Check for Ethernet hardware present
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
        Serial.println("Ethernet shield was not found.  Sorry, can't run without "
                       "hardware. :(");
        blockErr();
    }
    if (Ethernet.linkStatus() == LinkOFF) {
        Serial.println("Ethernet cable is not connected.");
        blockErr();
    }

#ifdef CONF_DHCP
    if (dhcpFailed) {
        Serial.println("DHCP conf failed");
        blockErr();
    }
#endif

    Serial.print("My ip is ");
    Ethernet.localIP().printTo(Serial);
    Serial.println();
    Serial.flush();

    // Init ntp client
    ntp::begin();
    setSyncProvider(ntp::getTime);

    if (timeStatus() == timeNotSet)
        Serial.println("Can't sync time");

    // TODO this should be in microseconds
    uint64_t usec = now();
    Serial.printf("%lu secs\n", usec);

    // start UDP
    Udp.begin(localPort);

    DefaultHeaderStruct header =
            DefaultHeaderStruct{ usec, 0, 0, samplingRateT::SR44, 16, 0, 0 };
    Udp.beginPacket("192.168.43.240", 8888);
    Udp.write((char*) &header, sizeof(DefaultHeaderStruct));
    Udp.endPacket();
}

void loop()
{
    // if there's data available, read a packet
    int packetSize = Udp.parsePacket();
    if (packetSize) {
        Serial.print("Received packet of size ");
        Serial.println(packetSize);
        Serial.print("From ");
        IPAddress remote = Udp.remoteIP();
        for (int i = 0; i < 4; i++) {
            Serial.print(remote[i], DEC);
            if (i < 3) {
                Serial.print(".");
            }
        }
        Serial.print(", port ");
        Serial.println(Udp.remotePort());

        // read the packet into packetBufffer
        Udp.read(packetBuffer, UDP_TX_PACKET_MAX_SIZE);
        Serial.println(packetBuffer);

        // send a reply to the IP address and port that sent us the packet we
        // received
        Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
        Udp.write(packetBuffer, packetSize);
        Udp.endPacket();
    }
    delay(10);
}
