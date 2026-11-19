#pragma once
#include <QUrl>
#include <QStringList>

// Optional module adapters use a local proxy, never a process-wide proxy.
inline QString localHttpProxy(const QString &value){
    const QUrl u(value.trimmed());
    if(!u.isValid()||u.scheme()!="http"||!QStringList{"127.0.0.1","::1","localhost"}.contains(u.host())||u.port()<1||u.port()>65535||!u.userInfo().isEmpty()||u.hasQuery()||u.hasFragment()||(!u.path().isEmpty()&&u.path()!="/"))return {};
    return u.toString(QUrl::RemovePath);
}
