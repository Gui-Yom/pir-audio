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
const uint8_t NUM_CHANNELS = 2;
// Can't use 2 channels and 512 samples due to a bug in the udp implementation (can't send packets bigger than 2048)
const uint16_t NUM_SAMPLES = 256;
// Number of buffers per channel
const uint8_t NUM_BUFFERS = NUM_SAMPLES / AUDIO_BLOCK_SAMPLES;
//endregion

// The UDP socket we use
EthernetUDP Udp;

// UDP remote port to send audio packets to
uint16_t remote_udp_port;

//region Audio system objects
// Audio shield driver
AudioControlSGTL5000 audioShield;
AudioOutputI2S out;
AudioInputI2S in;

// Audio circular buffers
AudioPlayQueue pql;
AudioPlayQueue pqr;
AudioRecordQueue rql;
AudioRecordQueue rqr;

// Audio system connections
AudioConnection outL(pql, 0, out, 0);
AudioConnection outR(pqr, 0, out, 1);
AudioConnection inL(in, 0, rql, 0);
AudioConnection inR(in, 1, rqr, 0);
//endregion

// Shorthand to block and do nothing
#define WAIT_INFINITE() while (true) yield();

// Size of the jacktrip packets
const uint32_t BUFFER_SIZE = PACKET_HEADER_SIZE + NUM_CHANNELS * NUM_SAMPLES * 2;
// Packet buffer (in/out)
uint8_t buffer[BUFFER_SIZE];

uint16_t seq = 0;
JacktripPacketHeader HEADER = JacktripPacketHeader{
        0,
        0,
        NUM_SAMPLES,
        samplingRateT::SR44,
        16,
        NUM_CHANNELS,
        NUM_CHANNELS };

//region Warning params
uint32_t last_receive = millis();
uint32_t last_perf_report = millis();
const uint32_t PERF_REPORT_DELAY = 3000;
//endregion

void setup()
{
    Ethernet.setSocketNum(4);
    if (BUFFER_SIZE > FNET_SOCKET_DEFAULT_SIZE) {
        Ethernet.setSocketSize(BUFFER_SIZE);
    }
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
        WAIT_INFINITE()
    }
    if (Ethernet.linkStatus() == LinkOFF) {
        Serial.println("Ethernet cable is not connected.");
        WAIT_INFINITE()
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

    Serial.printf("Packet size is %d bytes\n", BUFFER_SIZE);

    // Init ntp client
    // audio packets timestamps don't seem to be necessary
    /*
    ntp::begin();
    setSyncProvider(ntp::getTime);

    if (timeStatus() == timeNotSet)
        Serial.println("Can't sync time");

    uint64_t usec = now();
    Serial.printf("%lu secs\n", usec);
     */

    // Query jacktrip udp port
    EthernetClient c = EthernetClient();
    c.connect(peerAddress, REMOTE_TCP_PORT);

    uint32_t port = LOCAL_UDP_PORT;
    // port is sent little endian
    c.write((const uint8_t*) &port, 4);

    while (c.available() < 4) { }
    c.read((uint8_t*) &port, 4);
    remote_udp_port = port;
    Serial.printf("Remote port is %d\n", remote_udp_port);

    // start UDP
    Udp.begin(LOCAL_UDP_PORT);

    // + 10 for the audio input
    AudioMemory(NUM_BUFFERS * NUM_CHANNELS + 2 + 20);
    pql.setMaxBuffers(NUM_BUFFERS);
    pqr.setMaxBuffers(NUM_BUFFERS);
    Serial.printf("Allocated %d buffers\n", NUM_BUFFERS * NUM_CHANNELS + 2 + 20);

    audioShield.enable();
    audioShield.volume(0.5);
    audioShield.micGain(20);
    audioShield.inputSelect(AUDIO_INPUT_MIC);
    //audioShield.headphoneSelect(AUDIO_HEADPHONE_DAC);

    rql.begin();
    if (NUM_CHANNELS > 1) {
        rqr.begin();
    }

    AudioProcessorUsageMaxReset();
    AudioMemoryUsageMaxReset();
}

void loop()
{
    // TODO read osc parameters
    // TODO filter audio before transmitting

    // Send audio when we have enough samples to fill a packet
    while (rql.available() >= NUM_BUFFERS &&
           (NUM_CHANNELS == 1 || (NUM_CHANNELS > 1 && rqr.available() >= NUM_BUFFERS))) {
        HEADER.TimeStamp = seq;
        HEADER.SeqNumber = seq;
        memcpy(buffer, &HEADER, PACKET_HEADER_SIZE);
        uint8_t* pos = buffer + PACKET_HEADER_SIZE;
        for (int i = 0; i < NUM_BUFFERS; i++) {
            memcpy(pos, rql.readBuffer(),
                   AUDIO_BLOCK_SAMPLES * 2);
            rql.freeBuffer();
            pos += AUDIO_BLOCK_SAMPLES * 2;
        }
        if (NUM_CHANNELS > 1) {
            for (int i = 0; i < NUM_BUFFERS; i++) {
                memcpy(pos, rqr.readBuffer(), AUDIO_BLOCK_SAMPLES * 2);
                rqr.freeBuffer();
                pos += AUDIO_BLOCK_SAMPLES * 2;
            }
        }

        Udp.beginPacket(peerAddress, remote_udp_port);
        size_t written = Udp.write(buffer, BUFFER_SIZE);
        if (written != BUFFER_SIZE) {
            Serial.println("Net buffer is too small");
        }
        Udp.endPacket();

        /*
        if (rql.available() > 0) {
            Serial.printf("Running late behind (%d buffers)\n", rql.available());
        }*/

        seq += 1;
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
            rql.end();
            rqr.end();
            audioShield.disable();
            WAIT_INFINITE()
        } else if (size != BUFFER_SIZE) {
            Serial.println("Received a malformed packet");
        } else {
            // We received an audio packet
            //Serial.println("Received packet");
            Udp.read(buffer, BUFFER_SIZE);
            uint32_t remaining = pql.play((const int16_t*) (buffer + PACKET_HEADER_SIZE), NUM_SAMPLES);
            if (remaining > 0) {
                Serial.printf("%d samples dropped (L)", remaining);
            }
            if (NUM_CHANNELS > 1) {
                remaining = pqr.play((const int16_t*) (buffer + PACKET_HEADER_SIZE + NUM_SAMPLES * 2), NUM_SAMPLES);
                if (remaining > 0) {
                    Serial.printf("%d samples dropped (R)", remaining);
                }
            }
        }
    }

    if (millis() - last_receive > 1000) {
        Serial.println("We have not received anything for 1 second");
        last_receive = millis();
    }

    if (millis() - last_perf_report > PERF_REPORT_DELAY) {
        Serial.printf("Perf report : %d, %f %%\n", AudioMemoryUsage(), AudioProcessorUsage() * 100);
        last_perf_report = millis();
    }
}
