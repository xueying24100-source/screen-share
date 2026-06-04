/****************************************************************************
** Meta object code from reading C++ file 'annotationoverlay.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../src/ui/annotation/annotationoverlay.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'annotationoverlay.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.11.1. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN17AnnotationOverlayE_t {};
} // unnamed namespace

template <> constexpr inline auto AnnotationOverlay::qt_create_metaobjectdata<qt_meta_tag_ZN17AnnotationOverlayE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "AnnotationOverlay",
        "strokePacketReady",
        "",
        "StrokePacket",
        "pkt",
        "strokeFinished",
        "Stroke",
        "stroke",
        "undoRedoChanged",
        "textAnnotationCreated",
        "TextAnnotation",
        "text",
        "toolChanged",
        "AnnotationTool",
        "tool",
        "contentChanged",
        "closeRequested",
        "setPenColor",
        "QColor",
        "color",
        "setPenWidth",
        "width",
        "setEraserMode",
        "on",
        "setTextMode",
        "setFontSize",
        "pointSize",
        "setToolbarExcludeRect",
        "QRect",
        "r",
        "clearAll",
        "undo",
        "redo",
        "addStroke",
        "applyRemotePacket",
        "commitTextInput",
        "cancelTextInput"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'strokePacketReady'
        QtMocHelpers::SignalData<void(const StrokePacket &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Signal 'strokeFinished'
        QtMocHelpers::SignalData<void(const Stroke &)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 6, 7 },
        }}),
        // Signal 'undoRedoChanged'
        QtMocHelpers::SignalData<void()>(8, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'textAnnotationCreated'
        QtMocHelpers::SignalData<void(const TextAnnotation &)>(9, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 10, 11 },
        }}),
        // Signal 'toolChanged'
        QtMocHelpers::SignalData<void(AnnotationTool)>(12, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 13, 14 },
        }}),
        // Signal 'contentChanged'
        QtMocHelpers::SignalData<void()>(15, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'closeRequested'
        QtMocHelpers::SignalData<void()>(16, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'setPenColor'
        QtMocHelpers::SlotData<void(const QColor &)>(17, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 18, 19 },
        }}),
        // Slot 'setPenWidth'
        QtMocHelpers::SlotData<void(float)>(20, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Float, 21 },
        }}),
        // Slot 'setEraserMode'
        QtMocHelpers::SlotData<void(bool)>(22, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 23 },
        }}),
        // Slot 'setTextMode'
        QtMocHelpers::SlotData<void(bool)>(24, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 23 },
        }}),
        // Slot 'setFontSize'
        QtMocHelpers::SlotData<void(int)>(25, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 26 },
        }}),
        // Slot 'setToolbarExcludeRect'
        QtMocHelpers::SlotData<void(const QRect &)>(27, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 28, 29 },
        }}),
        // Slot 'clearAll'
        QtMocHelpers::SlotData<void()>(30, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'undo'
        QtMocHelpers::SlotData<void()>(31, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'redo'
        QtMocHelpers::SlotData<void()>(32, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'addStroke'
        QtMocHelpers::SlotData<void(const Stroke &)>(33, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 6, 7 },
        }}),
        // Slot 'applyRemotePacket'
        QtMocHelpers::SlotData<void(const StrokePacket &)>(34, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Slot 'commitTextInput'
        QtMocHelpers::SlotData<void()>(35, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'cancelTextInput'
        QtMocHelpers::SlotData<void()>(36, 2, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<AnnotationOverlay, qt_meta_tag_ZN17AnnotationOverlayE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject AnnotationOverlay::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN17AnnotationOverlayE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN17AnnotationOverlayE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN17AnnotationOverlayE_t>.metaTypes,
    nullptr
} };

void AnnotationOverlay::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<AnnotationOverlay *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->strokePacketReady((*reinterpret_cast<std::add_pointer_t<StrokePacket>>(_a[1]))); break;
        case 1: _t->strokeFinished((*reinterpret_cast<std::add_pointer_t<Stroke>>(_a[1]))); break;
        case 2: _t->undoRedoChanged(); break;
        case 3: _t->textAnnotationCreated((*reinterpret_cast<std::add_pointer_t<TextAnnotation>>(_a[1]))); break;
        case 4: _t->toolChanged((*reinterpret_cast<std::add_pointer_t<AnnotationTool>>(_a[1]))); break;
        case 5: _t->contentChanged(); break;
        case 6: _t->closeRequested(); break;
        case 7: _t->setPenColor((*reinterpret_cast<std::add_pointer_t<QColor>>(_a[1]))); break;
        case 8: _t->setPenWidth((*reinterpret_cast<std::add_pointer_t<float>>(_a[1]))); break;
        case 9: _t->setEraserMode((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 10: _t->setTextMode((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 11: _t->setFontSize((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 12: _t->setToolbarExcludeRect((*reinterpret_cast<std::add_pointer_t<QRect>>(_a[1]))); break;
        case 13: _t->clearAll(); break;
        case 14: _t->undo(); break;
        case 15: _t->redo(); break;
        case 16: _t->addStroke((*reinterpret_cast<std::add_pointer_t<Stroke>>(_a[1]))); break;
        case 17: _t->applyRemotePacket((*reinterpret_cast<std::add_pointer_t<StrokePacket>>(_a[1]))); break;
        case 18: _t->commitTextInput(); break;
        case 19: _t->cancelTextInput(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (AnnotationOverlay::*)(const StrokePacket & )>(_a, &AnnotationOverlay::strokePacketReady, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (AnnotationOverlay::*)(const Stroke & )>(_a, &AnnotationOverlay::strokeFinished, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (AnnotationOverlay::*)()>(_a, &AnnotationOverlay::undoRedoChanged, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (AnnotationOverlay::*)(const TextAnnotation & )>(_a, &AnnotationOverlay::textAnnotationCreated, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (AnnotationOverlay::*)(AnnotationTool )>(_a, &AnnotationOverlay::toolChanged, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (AnnotationOverlay::*)()>(_a, &AnnotationOverlay::contentChanged, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (AnnotationOverlay::*)()>(_a, &AnnotationOverlay::closeRequested, 6))
            return;
    }
}

const QMetaObject *AnnotationOverlay::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *AnnotationOverlay::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN17AnnotationOverlayE_t>.strings))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int AnnotationOverlay::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 20)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 20;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 20)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 20;
    }
    return _id;
}

// SIGNAL 0
void AnnotationOverlay::strokePacketReady(const StrokePacket & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void AnnotationOverlay::strokeFinished(const Stroke & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void AnnotationOverlay::undoRedoChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void AnnotationOverlay::textAnnotationCreated(const TextAnnotation & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}

// SIGNAL 4
void AnnotationOverlay::toolChanged(AnnotationTool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1);
}

// SIGNAL 5
void AnnotationOverlay::contentChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 5, nullptr);
}

// SIGNAL 6
void AnnotationOverlay::closeRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 6, nullptr);
}
QT_WARNING_POP
