#ifndef PIR_AUDIO_NTP_H
#define PIR_AUDIO_NTP_H

#include <TimeLib.h>

namespace ntp {
    void initNtp();
    time_t getNtpTime();
}

#endif //PIR_AUDIO_NTP_H
