#ifndef DATABASEHELPER_H
#define DATABASEHELPER_H

#include <QString>
#include <QVector>
#include "cameraworker.h"

class DatabaseHelper
{
public:
    static void initializeDatabase();
    static void saveShape(const ShapeData &shape);
    static QVector<ShapeData> getAllShapes();
    static void deleteShape(int shapeId);
};

#endif // DATABASEHELPER_H
