#include <NativeEthernet.h>
#include "PacketHeader.h"
#include <Audio.h>
#include <OSCBundle.h>

//region Network parameters

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xE0 };
// For manual ip address configuration
IPAddress ip(192, 168, 1, 2);
// Remote server ip address
IPAddress peerAddress(192, 168, 1, 1);
// Remote server tcp port
const uint16_t REMOTE_TCP_PORT = 4464;
// Local udp port to receive packets on
const uint16_t LOCAL_UDP_PORT = 8888;

// Set this to use the ip address from the dhcp
//#define CONF_DHCP
// Set this to manually configure the ip address
#define CONF_MANUAL

// Set this to wait for a serial connection before proceeding with the execution
#define WAIT_FOR_SERIAL

//endregion

//region Audio parameters
#define NUM_CHANNELS 1
// 128 is 22% cpu usage in stereo mode
// 256 is 24% cpu usage
// 512 is 24% cpu usage (mono)
#define NUM_SAMPLES 512
//endregion

// The UDP socket we use
EthernetUDP Udp;

// UDP remote port to send audio packets to
uint16_t remote_udp_port;

// Audio shield driver
AudioControlSGTL5000 audioShield;
// Audio circular buffer
AudioPlayQueue qL;
AudioPlayQueue qR;

#define WAIT_INFINITE while (true) yield();

// Open a tcp connection to exchange the udp ports with the server
void queryJacktripUdpPort()
{
    EthernetClient c = EthernetClient();
    c.connect(peerAddress, REMOTE_TCP_PORT);

    uint32_t port = LOCAL_UDP_PORT;
    c.write((const uint8_t*) &port, 4);

    while (c.available() < 4) { }
    c.read((uint8_t*) &port, 4);
    remote_udp_port = port;
    Serial.printf("Remote port is %d\n", remote_udp_port);
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
        WAIT_INFINITE
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

    uint64_t usec = now();
    Serial.printf("%lu secs\n", usec);
     */

    queryJacktripUdpPort();

    // start UDP
    Udp.begin(LOCAL_UDP_PORT);

    // Number of buffers per channel
    const uint8_t NUM_BUFFERS = NUM_SAMPLES / AUDIO_BLOCK_SAMPLES;

    AudioMemory(NUM_BUFFERS * NUM_CHANNELS + 2);
    Serial.printf("Allocated %d buffers\n", NUM_BUFFERS * NUM_CHANNELS + 2);
    audioShield.enable();
    audioShield.volume(0.5);
    qL.setMaxBuffers(NUM_BUFFERS);
    qR.setMaxBuffers(NUM_BUFFERS);

    AudioProcessorUsageMaxReset();
    AudioMemoryUsageMaxReset();
}

#define BUFFER_SIZE (PACKET_HEADER_SIZE + NUM_CHANNELS * NUM_SAMPLES * 2)

uint32_t last_send = 0;
uint32_t last_receive = millis();
// Packet buffer (in/out)
uint8_t buffer[BUFFER_SIZE];
uint16_t seq = 0;

//region Audio system links
AudioOutputI2S out;
AudioConnection connL(qL, 0, out, 0);
AudioConnection connR(qR, 0, out, 1);
//endregion

void sendAudioKeepalive()
{
    JacktripPacketHeader header = JacktripPacketHeader{
            seq,
            seq,
            NUM_SAMPLES,
            samplingRateT::SR44,
            16,
            NUM_CHANNELS,
            NUM_CHANNELS };
    Udp.beginPacket(peerAddress, remote_udp_port);
    memset(buffer, 0, BUFFER_SIZE);
    memcpy(buffer, &header, sizeof(JacktripPacketHeader));
    Udp.write(buffer, BUFFER_SIZE);
    Udp.endPacket();

    last_send = millis();
    seq += 1;
};

void loop()
{
    // TODO read osc parameters

    // TODO transmit audio
    // TODO filter audio before transmitting
    if (millis() - last_send > 500) {
        // Periodically tell the jacktrip server that we are still alive, so it still sends us data.
        sendAudioKeepalive();
    }

    int size;
    // Nonblocking
    if ((size = Udp.parsePacket())) {
        last_receive = millis();
        if (size == 63) { // Exit sequence
            // TODO verify that the packet is actually full of ones (not very important)

            Serial.println("Received exit packet");
            Serial.println("Perf statistics");
            Serial.printf("  maxmem: %d blocks\n", AudioMemoryUsageMax());
            Serial.printf("  maxcpu: %f %%\n", AudioProcessorUsageMax() * 100);

            // TODO we should add a restart strategy with a wakeup packet or something
            WAIT_INFINITE
        } else if (size != BUFFER_SIZE) {
            Serial.println("Received a malformed packet");
        } else {
            //Serial.println("Received packet");
            Udp.read(buffer, BUFFER_SIZE);
            uint32_t remaining = qL.play((const int16_t*) (buffer + PACKET_HEADER_SIZE), NUM_SAMPLES);
            if (remaining > 0) {
                Serial.printf("%d samples dropped (L)", remaining);
            }
            /*
            remaining = qR.play((const int16_t*) (buffer + PACKET_HEADER_SIZE + NUM_SAMPLES * 2), NUM_SAMPLES);
            if (remaining > 0) {
                Serial.printf("%d samples dropped (R)", remaining);
            }*/
        }
    }

    if (millis() - last_receive > 1000) {
        Serial.println("We have not received anything for 1 second");
        last_receive = millis();
    }
}
