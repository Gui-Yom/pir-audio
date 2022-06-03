#include <NativeEthernet.h>
#include <TimeLib.h>
#include "Ntp.h"
#include "PacketHeader.h"
#include "Audio.h"

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xE0 };
IPAddress ip(192, 168, 1, 2);
//IPAddress gateway(192, 168, 1, 1);

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

uint16_t remotePort;

AudioControlSGTL5000 audioShield;

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

    EthernetClient c = EthernetClient();
    c.connect(IPAddress(192, 168, 1, 1), 4464);

    uint8_t buf[4] = { 0xB8, 0x22, 0, 0 };
    c.write(buf, 4);

    while (c.available() < 4) {

    }

    int size = c.read(buf, 4);
    Serial.printf("read %d bytes\n", size);
    remotePort = (buf[1] << 8) + buf[0];
    Serial.printf("Remote port is %d\n", remotePort);

    // start UDP
    Udp.begin(8888);

    AudioMemory(2);
    audioShield.enable();
    audioShield.volume(0.4);
}

#define SAMPLES_SIZE 512
#define BUFFER_SIZE (16 + SAMPLES_SIZE * 2)

uint32_t last_send = 0;
uint8_t buffer[BUFFER_SIZE];
uint16_t seq = 0;

AudioOutputI2S out;
AudioPlayQueue q;
AudioConnection conn(q, 0, out, 0);

void loop()
{
    if (millis() - last_send > 500) {
        JacktripPacketHeader header =
                JacktripPacketHeader{ seq, seq, SAMPLES_SIZE, samplingRateT::SR44, 16, 1, 1 };
        Udp.beginPacket("192.168.1.1", remotePort);
        memcpy(buffer, &header, sizeof(JacktripPacketHeader));
        Udp.write(buffer, BUFFER_SIZE);
        Udp.endPacket();

        last_send = millis();
        seq += 1;
    }
    int size = 0;
    if ((size = Udp.parsePacket())) {
        if (size == 63) {
            Serial.println("Received exit packet");
            blockErr();
        }
        Udp.read(buffer, BUFFER_SIZE);
        q.play((const int16_t*) (buffer + 16), 512);
        Serial.println("Received packet");
    }
}
