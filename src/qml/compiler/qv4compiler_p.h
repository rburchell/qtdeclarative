/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#ifndef QV4COMPILER_P_H
#define QV4COMPILER_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtCore/qstring.h>
#include <QtCore/qhash.h>
#include <QtCore/qstringlist.h>
#include <private/qjson_p.h>
#include <private/qv4global_p.h>
#include <private/qqmljsastfwd_p.h>
#include <private/qv4compileddata_p.h>

QT_BEGIN_NAMESPACE

class QQmlPropertyData;

namespace QV4 {

namespace CompiledData {
struct Unit;
struct Lookup;
struct RegExp;
struct JSClassMember;
}

namespace Compiler {

struct Q_QML_PRIVATE_EXPORT StringTableGenerator {
    StringTableGenerator();

    int registerString(const QString &str);
    int getStringId(const QString &string) const;
    QString stringForIndex(int index) const { return strings.at(index); }
    uint stringCount() const { return strings.size(); }

    uint sizeOfTableAndData() const { return stringDataSize + strings.count() * sizeof(uint); }

    void clear();

    void serialize(CompiledData::Unit *unit);

private:
    QHash<QString, int> stringToId;
    QStringList strings;
    uint stringDataSize;
};

struct Q_QML_PRIVATE_EXPORT JSUnitGenerator {
    struct MemberInfo {
        QString name;
        bool isAccessor;
    };

    JSUnitGenerator(Module *module);

    int registerString(const QString &str) { return stringTable.registerString(str); }
    int getStringId(const QString &string) const { return stringTable.getStringId(string); }
    QString stringForIndex(int index) const { return stringTable.stringForIndex(index); }

    int registerGetterLookup(const QString &name);
    int registerGetterLookup(int nameIndex);
    int registerSetterLookup(const QString &name);
    int registerSetterLookup(int nameIndex);
    int registerGlobalGetterLookup(const QString &name);
    int registerGlobalGetterLookup(int nameIndex);
    int registerIndexedGetterLookup();
    int registerIndexedSetterLookup();

    int registerRegExp(QQmlJS::AST::RegExpLiteral *regexp);

    int registerConstant(ReturnedValue v);
    ReturnedValue constant(int idx);

    int registerJSClass(const QVector<MemberInfo> &members);
    int registerJSClass(int count, CompiledData::JSClassMember *members);

    enum GeneratorOption {
        GenerateWithStringTable,
        GenerateWithoutStringTable
    };

    QV4::CompiledData::Unit *generateUnit(GeneratorOption option = GenerateWithStringTable);
    // Returns bytes written
    void writeFunction(char *f, Context *irFunction) const;

    StringTableGenerator stringTable;
    QString codeGeneratorName;
private:
    CompiledData::Unit generateHeader(GeneratorOption option, QJsonPrivate::q_littleendian<quint32> *functionOffsets, uint *jsClassDataOffset);

    Module *module;

    QList<CompiledData::Lookup> lookups;
    QVector<CompiledData::RegExp> regexps;
    QVector<ReturnedValue> constants;
    QByteArray jsClassData;
    QVector<int> jsClassOffsets;
};

}

}

QT_END_NAMESPACE

#endif
