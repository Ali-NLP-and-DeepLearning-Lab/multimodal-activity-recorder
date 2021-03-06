#include "mmrfilemetadata.h"

#include "irqm/irqmpathhelper.h"
#include "irqm/irqmsqlitehelper.h"

#include "sqlite/sqlite3.h"
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
MMRFileMetadata::MMRFileMetadata(QObject *parent) : QObject(parent) {

}
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
void MMRFileMetadata::clear() {
    close();

    modalityIdentifierToIdxMap.clear();
}
//---------------------------------------------------------------------------
void MMRFileMetadata::close() {
    {
        QMutexLocker dbMutexLocker(&dbMutex);
        if (db) {
            sqlite3_close_v2(db);
            db = NULL;
        }
    }
}
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
void MMRFileMetadata::loadFromFileDirPath(QString path) {
    if (db) return;

    basePath = path;

    QString filePath = IRQMPathHelper::concatenate(path, MMR_FILE_METADATA_FILENAME);

    int rc = sqlite3_open_v2(filePath.toUtf8().constData(), &db, SQLITE_OPEN_READWRITE, NULL);
    if (rc != SQLITE_OK) {
        qDebug() << "failed to open sqlite file:" << filePath;
        return;
    }

    updateModalityIdentifierToIdxMap();
}
//---------------------------------------------------------------------------
void MMRFileMetadata::updateModalityIdentifierToIdxMap() {
    modalityIdentifierToIdxMap.clear();

    QVariantList modalities = getModalities();
    for (auto modalityVariant=modalities.begin(); modalityVariant!=modalities.end(); ++modalityVariant) {
        QVariantMap modality = (*modalityVariant).toMap();

        int modalityIdx = modality.value("modality_idx").toInt();
        QString identifier = modality.value("identifier").toString();

        modalityIdentifierToIdxMap.insert(identifier, modalityIdx);
    }
}
//---------------------------------------------------------------------------
QVariantList MMRFileMetadata::getModalities() {
    if (!db) return QVariantList();

    QVariantList modalities;

    QString query = "SELECT * FROM modalities";
    sqlite3_stmt *stmt = IRQMSQLiteHelper::prepare(db, query);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        QVariantMap row = IRQMSQLiteHelper::fetchRow(stmt);

        modalities.append(row);
    }

    sqlite3_finalize(stmt);

    return modalities;
}
//---------------------------------------------------------------------------
qint64 MMRFileMetadata::getLength() {
    if (!db) return 0;

    qint64 timestamp = 0;

    QString query = "SELECT timestamp FROM recordings ORDER BY timestamp DESC LIMIT 1";

    sqlite3_stmt *stmt = IRQMSQLiteHelper::prepare(db, query);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        QVariantMap row = IRQMSQLiteHelper::fetchRow(stmt);
        timestamp = row.value("timestamp").toLongLong();
    }
    sqlite3_finalize(stmt);

    return timestamp;
}
//---------------------------------------------------------------------------
qint64 MMRFileMetadata::getModalityDataPos(QString identifier, qint64 timestamp) {
    if (!db) return -1;

    if (!modalityIdentifierToIdxMap.contains(identifier)) return -1;
    int modalityIdx = modalityIdentifierToIdxMap.value(identifier);

    qint64 dataPos = 0;

    QString query = "SELECT * FROM recordings WHERE modality_idx = ? AND timestamp <= ? ORDER BY timestamp DESC LIMIT 1";

    sqlite3_stmt *stmt = IRQMSQLiteHelper::prepare(db, query);
    IRQMSQLiteHelper::bindValue(stmt, 1, modalityIdx);
    IRQMSQLiteHelper::bindValue(stmt, 2, timestamp);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        QVariantMap row = IRQMSQLiteHelper::fetchRow(stmt);
        dataPos = row.value("data_pos").toLongLong();
    }
    sqlite3_finalize(stmt);

    return dataPos;
}
//---------------------------------------------------------------------------
QVariantList MMRFileMetadata::getModalityRecordings(QString identifier) {
    if (!db) return QVariantList();

    if (!modalityIdentifierToIdxMap.contains(identifier)) return QVariantList();
    int modalityIdx = modalityIdentifierToIdxMap.value(identifier);

    QVariantList recordings;

    QString query = "SELECT * FROM recordings WHERE modality_idx = ? ORDER BY timestamp ASC";
    sqlite3_stmt *stmt = IRQMSQLiteHelper::prepare(db, query);
    IRQMSQLiteHelper::bindValue(stmt, 1, modalityIdx);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        QVariantMap row = IRQMSQLiteHelper::fetchRow(stmt);

        recordings.append(row);
    }

    sqlite3_finalize(stmt);

    return recordings;
}
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
void MMRFileMetadata::createToFileDirPath(QString path, bool inMemoryMode) {
    if (db) return;

    isInMemoryDb = inMemoryMode;

    QString filePath = IRQMPathHelper::concatenate(path, MMR_FILE_METADATA_FILENAME);
    dbFilePath = filePath;

    int rc = SQLITE_OK;
    if (inMemoryMode) {
        rc = sqlite3_open_v2(":memory:", &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    }
    else {
        rc = sqlite3_open_v2(filePath.toUtf8().constData(), &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    }
    if (rc != SQLITE_OK) {
        qDebug() << "failed to open or create sqlite file:" << filePath;
        return;
    }

    initializeTables();
}
//---------------------------------------------------------------------------
void MMRFileMetadata::addModality(QString type, QString identifier, QVariantMap configuration) {
    if (!db) return;

    // configuration to blob
    QByteArray configurationByteArray;
    QDataStream configurationDataStream(&configurationByteArray, QIODevice::WriteOnly);
    configurationDataStream << configuration;

    // sqlite3
    int modalityIdx = 0;
    {
        QMutexLocker dbMutexLocker(&dbMutex);

        QString query = "INSERT INTO modalities (type, identifier, configuration) VALUES (?, ?, ?)";
        sqlite3_stmt *stmt = IRQMSQLiteHelper::prepare(db, query);
        IRQMSQLiteHelper::bindValue(stmt, 1, type);
        IRQMSQLiteHelper::bindValue(stmt, 2, identifier);
        IRQMSQLiteHelper::bindValue(stmt, 3, configurationByteArray);

        if (sqlite3_step(stmt) == SQLITE_DONE) {
            modalityIdx = sqlite3_last_insert_rowid(db);
        }
        sqlite3_finalize(stmt);
    }

    // modalityIdentifierToIdxMap
    modalityIdentifierToIdxMap.insert(identifier, modalityIdx);
}
//---------------------------------------------------------------------------
void MMRFileMetadata::addRecording(QString identifier, qint64 dataPos, qint64 timestamp) {
    if (!db) return;

    // modalityIdx
    if (!modalityIdentifierToIdxMap.contains(identifier)) return;
    int modalityIdx = modalityIdentifierToIdxMap.value(identifier);

    // sqlite3
    {
        QMutexLocker dbMutexLocker(&dbMutex);

        QString query = "INSERT INTO recordings (modality_idx, data_pos, timestamp) VALUES (?, ?, ?)";
        sqlite3_stmt *stmt = IRQMSQLiteHelper::prepare(db, query);
        IRQMSQLiteHelper::bindValue(stmt, 1, modalityIdx);
        IRQMSQLiteHelper::bindValue(stmt, 2, dataPos);
        IRQMSQLiteHelper::bindValue(stmt, 3, timestamp);

        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}
//---------------------------------------------------------------------------
void MMRFileMetadata::beginTransaction() {
    dbTransactionMutex.lock();
    sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
}
//---------------------------------------------------------------------------
void MMRFileMetadata::commitTransaction() {
    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
    dbTransactionMutex.unlock();
}
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
void MMRFileMetadata::finalizeWriting() {
    if (!db) return;

    if (isInMemoryDb) {
        sqlite3 *fileDb;
        int rc;

        rc = sqlite3_open(dbFilePath.toUtf8().constData(), &fileDb);
        if (rc == SQLITE_OK) {
            sqlite3_backup *fileDbBackup;
            fileDbBackup = sqlite3_backup_init(fileDb, "main", db, "main");
            if (fileDbBackup) {
                do {
                    rc = sqlite3_backup_step(fileDbBackup, 10);
                    if (rc == SQLITE_BUSY || rc == SQLITE_LOCKED) {
                      sqlite3_sleep(250);
                    }
                } while (rc == SQLITE_OK || rc == SQLITE_BUSY || rc == SQLITE_LOCKED);
            }
            sqlite3_backup_finish(fileDbBackup);
            sqlite3_close(fileDb);
        }
    }
}
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
void MMRFileMetadata::initializeTables() {
    if (!db) return;

    QString queryString;


    // table creation
    queryString = "CREATE TABLE IF NOT EXISTS db_info ( "
            "key TEXT NOT NULL, "
            "value BLOB "
            ")";
    sqlite3_exec(db, queryString.toUtf8().constData(), 0, 0, NULL);

    queryString = "CREATE TABLE IF NOT EXISTS modalities ( "
            "modality_idx INTEGER PRIMARY KEY AUTOINCREMENT, "
            "type TEXT, "
            "identifier TEXT, "
            "configuration BLOB "
            ")";
    sqlite3_exec(db, queryString.toUtf8().constData(), 0, 0, NULL);

    queryString = "CREATE TABLE IF NOT EXISTS recordings ( "
            "recording_idx INTEGER PRIMARY KEY AUTOINCREMENT, "
            "modality_idx INTEGER NOT NULL, "
            "data_pos INTEGER, "
            "timestamp INTEGER "
            ")";
    sqlite3_exec(db, queryString.toUtf8().constData(), 0, 0, NULL);


    // index creation
    queryString = "CREATE INDEX IF NOT EXISTS "
            "recordings_modality_index_timestamp ON recordings "
            "(modality_idx, timestamp)";
    sqlite3_exec(db, queryString.toUtf8().constData(), 0, 0, NULL);


    // add basic db info
    {
        QString query = "INSERT INTO db_info (key, value) VALUES (?, ?)";
        sqlite3_stmt *stmt = IRQMSQLiteHelper::prepare(db, query);
        IRQMSQLiteHelper::bindValue(stmt, 1, QString("app_version"));
        IRQMSQLiteHelper::bindValue(stmt, 2, QString(APP_VERSION_STRING));
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    {
        QString query = "INSERT INTO db_info (key, value) VALUES (?, CURRENT_TIMESTAMP)";
        sqlite3_stmt *stmt = IRQMSQLiteHelper::prepare(db, query);
        IRQMSQLiteHelper::bindValue(stmt, 1, QString("created_date"));
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
