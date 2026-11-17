#pragma once
#include <QAbstractListModel>
#include <QJsonArray>
#include <QJsonObject>
#include <QVariantMap>

// A stable display model. Favourite changes update one row; insertions/removals
// preserve surviving delegates and scroll anchors instead of resetting a view.
class StreamListModel:public QAbstractListModel {
public:
    explicit StreamListModel(QObject *parent=nullptr):QAbstractListModel(parent){}
    int rowCount(const QModelIndex &parent={})const override{return parent.isValid()?0:m_rows.size();}
    QVariant data(const QModelIndex &index,int role)const override {
        if(!index.isValid()||index.row()<0||index.row()>=m_rows.size()||role!=Qt::UserRole+1)return {};
        return m_rows[index.row()].toObject().toVariantMap();
    }
    QHash<int,QByteArray> roleNames()const override{return {{Qt::UserRole+1,"entry"}};}
    void setRows(const QJsonArray &rows){
        auto id=[](const QJsonValue &v){return v.toObject().value("id").toString();};
        int prefix=0,suffix=0;
        while(prefix<m_rows.size()&&prefix<rows.size()&&id(m_rows[prefix])==id(rows[prefix]))++prefix;
        while(suffix<m_rows.size()-prefix&&suffix<rows.size()-prefix&&id(m_rows[m_rows.size()-1-suffix])==id(rows[rows.size()-1-suffix]))++suffix;
        const int remove=m_rows.size()-prefix-suffix,insert=rows.size()-prefix-suffix;
        if(remove){beginRemoveRows({},prefix,prefix+remove-1);QJsonArray kept;for(int i=0;i<prefix;++i)kept.append(m_rows[i]);for(int i=m_rows.size()-suffix;i<m_rows.size();++i)kept.append(m_rows[i]);m_rows=kept;endRemoveRows();}
        if(insert){beginInsertRows({},prefix,prefix+insert-1);QJsonArray next;for(int i=0;i<prefix;++i)next.append(m_rows[i]);for(int n=0;n<insert;++n)next.append(rows[prefix+n]);for(int i=prefix;i<m_rows.size();++i)next.append(m_rows[i]);m_rows=next;endInsertRows();}
        for(int i=0;i<rows.size();++i)if(m_rows[i]!=rows[i]){m_rows[i]=rows[i];emit dataChanged(index(i),index(i),{Qt::UserRole+1});}
    }
private:
    QJsonArray m_rows;
};
