#ifndef SESSIONMANAGER_H
#define SESSIONMANAGER_H

#include <QObject>
#include <QList>

#include "Database.h"

class SessionManager : public QObject
{
    Q_OBJECT

public:

    explicit SessionManager(QSettings* settings, QObject *parent = nullptr);
    ~SessionManager();

public slots:

    void logDutInfo(Dut dut);
    void clear();
    void writeDutRecordsToDatabase();

    QString operatorName() const {return _operatorName;}
    QString startTime() const {return _startTime;}
    QString batchNumber() const {return _batchNumber;}
    QString batchInfo() const {return _batchInfo;}
    int successCount() const {return _successCount;}
    int failedCount() const {return _failedCount;}

    void setOperatorName(const QString& text) {_operatorName = text;}
    void setStartTime(const QString& text) {_startTime = text;}
    void setBatchNumber(const QString& text) {_batchNumber = text;}
    void setBatchInfo(const QString& text) { _batchInfo = text;}

signals:

    void sessionStatsChanged();

private:

//    QMap<QString, QVariant> _properties =
//    {
//        {"operatorName", QString()},
//        {"startTime", QString()},
//        {"batchNumber", QString()},
//        {"batchInfo", QString()},
//        {"successCount", 0},
//        {"failedCount", 0}
//    };

        QString _operatorName = "";
        QString _startTime = "";
        QString _batchNumber = "";
        QString _batchInfo = "";
        int _successCount = 0;
        int _failedCount = 0;

        QList<DutRecord> _records;

        DataBase *_db;
};

#endif // SESSIONMANAGER_H
