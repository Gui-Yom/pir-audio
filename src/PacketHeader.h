#ifndef PIR_AUDIO_PACKETHEADER_H
#define PIR_AUDIO_PACKETHEADER_H

enum audioBitResolutionT
{
    BIT8 = 1,  ///< 8 bits
    BIT16 = 2, ///< 16 bits (default)
    BIT24 = 3, ///< 24 bits
    BIT32 = 4  ///< 32 bits
};

enum samplingRateT
{
    SR22,  ///<  22050 Hz
    SR32,  ///<  32000 Hz
    SR44,  ///<  44100 Hz
    SR48,  ///<  48000 Hz
    SR88,  ///<  88200 Hz
    SR96,  ///<  96000 Hz
    SR192, ///< 192000 Hz
    UNDEF  ///< Undefined
};

struct DefaultHeaderStruct
{
public:
    uint64_t TimeStamp;    ///< Time Stamp
    uint16_t SeqNumber;    ///< Sequence Number
    uint16_t BufferSize;   ///< Buffer Size in Samples
    uint8_t SamplingRate;  ///< Sampling Rate in JackAudioInterface::samplingRateT
    uint8_t BitResolution; ///< Audio Bit Resolution
    uint8_t NumIncomingChannelsFromNet; ///< Number of incoming Channels from the
    ///< network
    uint8_t
            NumOutgoingChannelsToNet; ///< Number of outgoing Channels to the network
};

#endif //PIR_AUDIO_PACKETHEADER_H
