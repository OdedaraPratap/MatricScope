#include "databasehelper.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QDir>
#include <QCoreApplication>
#include <QThread>

namespace {

QSqlDatabase databaseForCurrentThread()
{
    const bool isApplicationThread = QCoreApplication::instance() &&
        QThread::currentThread() == QCoreApplication::instance()->thread();
    const QString connectionName = isApplicationThread
        ? QStringLiteral("qt_sql_default_connection")
        : QStringLiteral("MatricScopeWorker_%1")
              .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()), 0, 16);

    QSqlDatabase db = QSqlDatabase::contains(connectionName)
        ? QSqlDatabase::database(connectionName)
        : QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);

    if (db.databaseName().isEmpty()) {
        db.setDatabaseName(QDir::current().absoluteFilePath(QStringLiteral("History.db")));
    }
    if (!db.isOpen() && !db.open()) {
        qWarning() << "Could not open database connection" << connectionName
                   << db.lastError().text();
        return db;
    }

    QSqlQuery configure(db);
    configure.exec(QStringLiteral("PRAGMA busy_timeout = 5000;"));
    return db;
}

} // namespace

void DatabaseHelper::initializeDatabase()
{
    QSqlDatabase db = databaseForCurrentThread();
    if (!db.isOpen()) {
        qDebug() << "Error: Failed to connect database." << db.lastError().text();
        return;
    }

    QSqlQuery query(db);
    query.exec(QStringLiteral("PRAGMA journal_mode = WAL;"));
    QString sql = R"(
        CREATE TABLE IF NOT EXISTS CustomShapes (
            Id INTEGER PRIMARY KEY AUTOINCREMENT,
            Name TEXT NOT NULL,
            ImagePath TEXT,
            W1X REAL, W1Y REAL, W2X REAL, W2Y REAL,
            L1X REAL, L1Y REAL, L2X REAL, L2Y REAL,
            RefAngle REAL,
            ContourData TEXT,
            TemplateMaskPath TEXT,
            SnapToEdge INTEGER DEFAULT 0
        );
    )";

    if (!query.exec(sql)) {
        qDebug() << "Error creating table:" << query.lastError().text();
    }

    // Safe column migrations (mirroring C# try/catch blocks)
    query.exec("ALTER TABLE CustomShapes ADD COLUMN TemplateMaskPath TEXT;");
    query.exec("ALTER TABLE CustomShapes ADD COLUMN ContourData TEXT;");
    query.exec("ALTER TABLE CustomShapes ADD COLUMN RefAngle REAL DEFAULT 0;");
    query.exec("ALTER TABLE CustomShapes ADD COLUMN SnapToEdge INTEGER DEFAULT 0;");
}

void DatabaseHelper::saveShape(const ShapeData &shape)
{
    QSqlDatabase db = databaseForCurrentThread();
    if (!db.isOpen()) return;

    QSqlQuery query(db);
    query.prepare(R"(
        INSERT INTO CustomShapes (Name, ImagePath, W1X, W1Y, W2X, W2Y, L1X, L1Y, L2X, L2Y, RefAngle, ContourData, TemplateMaskPath, SnapToEdge)
        VALUES (:Name, :ImagePath, :W1X, :W1Y, :W2X, :W2Y, :L1X, :L1Y, :L2X, :L2Y, :RefAngle, :ContourData, :TemplateMaskPath, :SnapToEdge);
    )");

    query.bindValue(":Name", shape.Name);
    query.bindValue(":ImagePath", shape.ImagePath);
    query.bindValue(":W1X", shape.WidthPt1.x);
    query.bindValue(":W1Y", shape.WidthPt1.y);
    query.bindValue(":W2X", shape.WidthPt2.x);
    query.bindValue(":W2Y", shape.WidthPt2.y);
    query.bindValue(":L1X", shape.LengthPt1.x);
    query.bindValue(":L1Y", shape.LengthPt1.y);
    query.bindValue(":L2X", shape.LengthPt2.x);
    query.bindValue(":L2Y", shape.LengthPt2.y);
    query.bindValue(":RefAngle", shape.RefAngle);
    query.bindValue(":ContourData", shape.ContourData);
    query.bindValue(":TemplateMaskPath", shape.TemplateMaskPath);
    query.bindValue(":SnapToEdge", shape.SnapToEdge ? 1 : 0);

    if (!query.exec()) {
        qDebug() << "Error saving custom shape:" << query.lastError().text();
    }
}

QVector<ShapeData> DatabaseHelper::getAllShapes()
{
    QVector<ShapeData> list;
    QSqlDatabase db = databaseForCurrentThread();
    if (!db.isOpen()) return list;

    QSqlQuery query(QStringLiteral("SELECT * FROM CustomShapes;"), db);
    while (query.next()) {
        ShapeData shape;
        shape.Id = query.value("Id").toInt();
        shape.Name = query.value("Name").toString();
        shape.ImagePath = query.value("ImagePath").toString();
        shape.WidthPt1 = cv::Point2f(query.value("W1X").toFloat(), query.value("W1Y").toFloat());
        shape.WidthPt2 = cv::Point2f(query.value("W2X").toFloat(), query.value("W2Y").toFloat());
        shape.LengthPt1 = cv::Point2f(query.value("L1X").toFloat(), query.value("L1Y").toFloat());
        shape.LengthPt2 = cv::Point2f(query.value("L2X").toFloat(), query.value("L2Y").toFloat());
        shape.RefAngle = query.value("RefAngle").toFloat();
        shape.ContourData = query.value("ContourData").toString();
        shape.TemplateMaskPath = query.value("TemplateMaskPath").toString();
        shape.SnapToEdge = query.value("SnapToEdge").toInt() == 1;

        list.append(shape);
    }
    return list;
}

void DatabaseHelper::deleteShape(int shapeId)
{
    QSqlDatabase db = databaseForCurrentThread();
    if (!db.isOpen()) return;

    QSqlQuery query(db);
    query.prepare("DELETE FROM CustomShapes WHERE Id = :Id;");
    query.bindValue(":Id", shapeId);
    if (!query.exec()) {
        qDebug() << "Error deleting custom shape:" << query.lastError().text();
    }
}
