#ifndef PIR_AUDIO_NTP_H
#define PIR_AUDIO_NTP_H

#include <TimeLib.h>

namespace ntp {
    void begin();
    time_t getTime();
}

#endif //PIR_AUDIO_NTP_H
