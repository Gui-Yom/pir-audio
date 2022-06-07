#include <NativeEthernet.h>
#include <TimeLib.h>
#include "PacketHeader.h"
#include "Audio.h"

#define WAIT_INFINITE while (true) yield();

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xE0 };
// For manual ip address configuration
IPAddress ip(192, 168, 1, 2);
IPAddress peerAddress(192, 168, 1, 1);
const uint16_t REMOTE_TCP_PORT = 4464;
const uint16_t LOCAL_UDP_PORT = 8888;

// Set this to use the ip address from the dhcp
//#define CONF_DHCP
// Set this to manually configure the ip address
#define CONF_MANUAL

// Set this to wait for a serial connection before proceeding with the execution
#define WAIT_FOR_SERIAL

// The UDP socket we use
EthernetUDP Udp;

// UDP remote port to send audio packets to
uint16_t remote_udp_port;

AudioControlSGTL5000 audioShield;
AudioPlayQueue q;

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
    while (!Serial) yield();
#endif

    // Check for Ethernet hardware present
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
        Serial.println("Ethernet shield was not found.  Sorry, can't run without "
                       "hardware. :(");
        WAIT_INFINITE
    }
    if (Ethernet.linkStatus() == LinkOFF) {
        Serial.println("Ethernet cable is not connected.");
        WAIT_INFINITE
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

    // Init ntp client
    /*
    ntp::begin();
    setSyncProvider(ntp::getTime);

    if (timeStatus() == timeNotSet)
        Serial.println("Can't sync time");

    // TODO this should be in microseconds
    uint64_t usec = now();
    Serial.printf("%lu secs\n", usec);
     */

    EthernetClient c = EthernetClient();
    c.connect(peerAddress, REMOTE_TCP_PORT);

    uint32_t port = LOCAL_UDP_PORT;
    c.write((const uint8_t*) &port, 4);

    while (c.available() < 4) { }
    c.read((uint8_t*) &port, 4);
    remote_udp_port = port;
    Serial.printf("Remote port is %d\n", remote_udp_port);

    // start UDP
    Udp.begin(LOCAL_UDP_PORT);

    const uint8_t NUM_BUFFERS = 6;

    AudioMemory(NUM_BUFFERS + 2);
    audioShield.enable();
    audioShield.volume(0.5);
    q.setMaxBuffers(NUM_BUFFERS);
}

#define SAMPLES_SIZE 512
#define BUFFER_SIZE (PACKET_HEADER_SIZE + SAMPLES_SIZE * 2)

uint32_t last_send = 0;
uint8_t buffer[BUFFER_SIZE];
uint16_t seq = 0;

AudioOutputI2S out;
AudioConnection conn0(q, 0, out, 0);

void sendAudioKeepalive();

void loop()
{
    if (millis() - last_send > 500) {
        // Periodically tell the jacktrip server that we are still alive, so it still sends us data.
        sendAudioKeepalive();
    }

    int size;
    if ((size = Udp.parsePacket())) {
        if (size == 63) { // Exit sequence
            // TODO verify that the packet is actually full of ones (not very important)

            Serial.println("Received exit packet");
            Serial.println("Audio memory statistics");
            Serial.printf("  maxmem: %d blocks\n", AudioMemoryUsageMax());
            Serial.printf("  maxcpu: %f %%\n", AudioProcessorUsageMax() * 100);

            WAIT_INFINITE
        } else if (size != BUFFER_SIZE) {
            Serial.println("Received a malformed packet");
        } else {
            Udp.read(buffer, BUFFER_SIZE);
            q.play((const int16_t*) (buffer + PACKET_HEADER_SIZE), SAMPLES_SIZE);
            //Serial.println("Received packet");
        }
    }
}

void sendAudioKeepalive()
{
    JacktripPacketHeader header =
            JacktripPacketHeader{ seq, seq, SAMPLES_SIZE, samplingRateT::SR44, 16, 1, 1 };
    Udp.beginPacket(peerAddress, remote_udp_port);
    memset(buffer, 0, BUFFER_SIZE);
    memcpy(buffer, &header, sizeof(JacktripPacketHeader));
    Udp.write(buffer, BUFFER_SIZE);
    Udp.endPacket();

    last_send = millis();
    seq += 1;
};
