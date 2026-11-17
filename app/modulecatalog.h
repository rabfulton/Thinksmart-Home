#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>
#include <QVariantList>

// Trusted native registrations. Layout files select IDs, never code or paths.
class ModuleCatalog {
public:
    struct Entry {QString id,title,description,widget,page,actionPrefix;bool defaultEnabled=true;};
    static QList<Entry> entries(){return {
        {"assistant","Assistant","Voice conversations and local Sheila detection","assistant",{},{}},
        {"radio","Radio","Favourites and worldwide internet stations","radio",{},"radio."},
        {"weather","Weather","Local weather and forecasts","weather",{},"weather."},
        {"news","News","World, finance and Hacker News",{},"news",{}},
        {"chess","Chess","Local chess game and engine",{},"chess",{}},
        {"notes","Notes","Notes, dictation and personal actions",{},"notes","notes."},
        {"bluetooth","Bluetooth speaker","Phone audio; needs BlueZ and BlueALSA","music",{},{}},
        {"vrm","Victron energy","Battery and solar data; needs VRM credentials","energy",{},"battery."},
        {"navidrome","Music library","Local music; needs Navidrome server settings","music",{},{}},
        {"jellyfin","Video library","Films and series; needs Jellyfin server settings","video",{},"video."},
        {"iptv","IPTV","Live internet television and saved channels","iptv","iptv","iptv.",false}
    };}
    static QStringList names(){QStringList r;for(const auto &e:entries())r<<e.id;return r;}
    static bool defaultEnabled(const QString &id){for(const auto &e:entries())if(e.id==id)return e.defaultEnabled;return false;}
    static QString title(const QString &id){for(const auto &e:entries())if(e.id==id)return e.title;return id;}
    static QStringList widgets(){QStringList r;for(const auto &e:entries())if(!e.widget.isEmpty()&&!r.contains(e.widget))r<<e.widget;return r;}
    static QStringList fullPages(){QStringList r;for(const auto &e:entries())if(!e.page.isEmpty())r<<e.page;return r;}
    static bool widgetEnabled(const QString &id,const QVariantMap &modules){for(const auto &e:entries())if(e.widget==id&&modules.value(e.id).toBool())return true;return false;}
    static QVariantList options(){QVariantList r;for(const auto &e:entries())r<<QVariantMap{{"id",e.id},{"title",e.title},{"description",e.description}};return r;}
    static QVariantList views(){QVariantList r;for(const auto &id:widgets())r<<QVariantMap{{"id",id},{"title",id=="energy"?"Energy":id=="music"?"Music":id=="video"?"Video":title(id)}};return r;}
};
