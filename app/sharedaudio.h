#pragma once
#include <QFileInfo>
#include <QtGlobal>
inline bool sharedAudioEnabled(){
#ifdef THINKSMART_VOICE_TESTING
    return qEnvironmentVariableIntValue("THINKSMART_SHARED_AUDIO")==1;
#else
    return QFileInfo::exists("/etc/thinksmart/shared-audio.enabled");
#endif
}
