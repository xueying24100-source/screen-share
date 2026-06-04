#pragma once

#include <QColor>
#include <QMetaType>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QtGlobal>
#include <QVector>

enum class AnnotationTool {
    Pen = 0,
    Rectangle,
    Arrow,
    Text,
    Clear
};

enum class AnnotationAction {
    Begin = 0,
    Update,
    Commit,
    ClearAll
};

struct AnnotationPoint {
    float x = 0.0f;
    float y = 0.0f;
};

struct AnnotationStyle {
    QColor color = QColor(QStringLiteral("#ff3b30"));
    float width = 3.0f;
};

struct AnnotationCommand {
    QString commandId;
    QString objectId;
    QString userId;
    AnnotationTool tool = AnnotationTool::Pen;
    AnnotationAction action = AnnotationAction::Begin;
    QVector<AnnotationPoint> points;
    QRectF normalizedRect;
    QString text;
    AnnotationStyle style;
    QSizeF sourceCanvasSize;
    qint64 timestampMs = 0;
};

inline AnnotationPoint annotationPointFromQPointF(const QPointF &point) {
    AnnotationPoint normalizedPoint;
    normalizedPoint.x = static_cast<float>(point.x());
    normalizedPoint.y = static_cast<float>(point.y());
    return normalizedPoint;
}

inline QPointF qPointFFromAnnotationPoint(const AnnotationPoint &point) {
    return QPointF(point.x, point.y);
}

inline bool isValidAnnotationPoint(const AnnotationPoint &point) {
    return point.x >= 0.0f && point.x <= 1.0f &&
           point.y >= 0.0f && point.y <= 1.0f;
}

Q_DECLARE_METATYPE(AnnotationTool)
Q_DECLARE_METATYPE(AnnotationAction)
Q_DECLARE_METATYPE(AnnotationPoint)
Q_DECLARE_METATYPE(AnnotationStyle)
Q_DECLARE_METATYPE(AnnotationCommand)
Q_DECLARE_METATYPE(QVector<AnnotationCommand>)
