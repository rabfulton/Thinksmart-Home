#pragma once
#include <QString>
#include <QVariantList>
#include <QMap>

// Files are reloaded at conversation boundaries. Their contents never appear in
// the UI, local status API or logs; only names and the selected ID are exposed.
class AssistantPrompts {
public:
    static QString directory();
    void reload(const QString &selected,bool live=false);
    QString selected()const{return m_selected;}
    QVariantList personalities()const{return m_personalities;}
    QString problem()const{return m_problem;}
    QString instructions()const{return m_instructions;}
    QString liveInstructions()const{return m_liveInstructions;}
    QString backendInstructions()const{return m_backendInstructions;}
    bool ready()const{return m_problem.isEmpty()&&!m_selected.isEmpty();}
private:
    QString m_selected,m_problem,m_instructions,m_liveInstructions,m_backendInstructions;
    QVariantList m_personalities;
};
