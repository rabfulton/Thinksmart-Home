#pragma once
#include <QString>
#include <QList>

// Explicit public feed URLs. Configuration stores trusted IDs, never URLs/code.
struct NewsFeed {
    QString id,title,provider,url;
    static QList<NewsFeed> all(){return {
        {"WORLD_FINANCE","World & finance","Google News",{}},
        {"WORLD","International","Google News",{}},
        {"FINANCE","Global finance","Google News",{}},
        {"BUSINESS","Business","Google News",{}},
        {"TECHNOLOGY","Technology","Google News",{}},
        {"SCIENCE","Science","Google News",{}},
        {"BBC_WORLD","BBC World","BBC News","https://feeds.bbci.co.uk/news/world/rss.xml"},
        {"BBC_UK","BBC UK","BBC News","https://feeds.bbci.co.uk/news/uk/rss.xml"},
        {"BBC_SCOTLAND","BBC Scotland","BBC News","https://feeds.bbci.co.uk/news/scotland/rss.xml"},
        {"BBC_BUSINESS","BBC Business","BBC News","https://feeds.bbci.co.uk/news/business/rss.xml"},
        {"BBC_TECH","BBC Technology","BBC News","https://feeds.bbci.co.uk/news/technology/rss.xml"},
        {"GUARDIAN_WORLD","Guardian World","The Guardian","https://www.theguardian.com/world/rss"},
        {"GUARDIAN_BUSINESS","Guardian Business","The Guardian","https://www.theguardian.com/business/rss"},
        {"GUARDIAN_ENVIRONMENT","Guardian Environment","The Guardian","https://www.theguardian.com/environment/rss"},
        {"ALJAZEERA","Al Jazeera English","Al Jazeera","https://www.aljazeera.com/xml/rss/all.xml"},
        {"FRANCE24","France 24 English","France 24","https://www.france24.com/en/rss"},
        {"NPR_WORLD","NPR World","NPR","https://feeds.npr.org/1004/rss.xml"},
        {"CNBC_WORLD","CNBC World","CNBC","https://www.cnbc.com/id/100727362/device/rss/rss.html"},
        {"ARS","Ars Technica","Ars Technica","https://feeds.arstechnica.com/arstechnica/index"},
        {"NASA","NASA","NASA","https://www.nasa.gov/feed/"},
        {"NYT_WORLD","New York Times World","The New York Times","https://rss.nytimes.com/services/xml/rss/nyt/World.xml"}
    };}
    static bool contains(const QString &id){for(const auto &f:all())if(f.id==id)return true;return false;}
    static NewsFeed find(const QString &id){for(const auto &f:all())if(f.id==id)return f;return {};}
};
