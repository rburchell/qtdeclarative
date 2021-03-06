/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
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


#include "qsgcompressedtexture_p.h"
#include <QOpenGLContext>
#include <QOpenGLTexture>
#include <QOpenGLFunctions>
#include <QDebug>
#include <QtQuick/private/qquickwindow_p.h>

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(QSG_LOG_TEXTUREIO, "qt.scenegraph.textureio");

bool QSGCompressedTextureData::isValid() const
{
    if (data.isNull() || size.isEmpty() || !format)
        return false;
    if (dataLength < 0 || dataOffset < 0 || dataOffset >= data.length())
        return false;
    if (dataLength > 0 && qint64(dataOffset) + qint64(dataLength) > qint64(data.length()))
        return false;

    return true;
}

int QSGCompressedTextureData::sizeInBytes() const
{
    if (!isValid())
        return 0;
    return dataLength > 0 ? dataLength : data.length() - dataOffset;
}

Q_QUICK_PRIVATE_EXPORT QDebug operator<<(QDebug dbg, const QSGCompressedTextureData *d)
{
    QDebugStateSaver saver(dbg);

    dbg.nospace() << "QSGCompressedTextureData(";
    if (d) {
        dbg << d->logName << ' ';
        dbg << static_cast<QOpenGLTexture::TextureFormat>(d->format)
            << "[0x" << hex << d->format << dec << "]";
        dbg.space() << (d->hasAlpha ? "with" : "no") << "alpha" << d->size
                    << "databuffer" << d->data.size() << "offset" << d->dataOffset << "length";
        dbg.nospace() << d->dataLength << ")";
    } else {
        dbg << "null)";
    }
    return dbg;
}

QSGCompressedTexture::QSGCompressedTexture(const DataPtr& texData)
    : m_textureData(texData)
{
    if (m_textureData) {
        m_size = m_textureData->size;
        m_hasAlpha = m_textureData->hasAlpha;
    }
}

QSGCompressedTexture::~QSGCompressedTexture()
{
#if QT_CONFIG(opengl)
    if (m_textureId) {
        QOpenGLContext *ctx = QOpenGLContext::currentContext();
        QOpenGLFunctions *funcs = ctx ? ctx->functions() : nullptr;
        if (!funcs)
            return;

        funcs->glDeleteTextures(1, &m_textureId);
    }
#endif
}

int QSGCompressedTexture::textureId() const
{
#if QT_CONFIG(opengl)
    if (!m_textureId) {
        QOpenGLContext *ctx = QOpenGLContext::currentContext();
        QOpenGLFunctions *funcs = ctx ? ctx->functions() : nullptr;
        if (!funcs)
            return 0;

        funcs->glGenTextures(1, &m_textureId);
    }
#endif
    return m_textureId;
}

QSize QSGCompressedTexture::textureSize() const
{
    return m_size;
}

bool QSGCompressedTexture::hasAlphaChannel() const
{
    return m_hasAlpha;
}

bool QSGCompressedTexture::hasMipmaps() const
{
    return false;
}

void QSGCompressedTexture::bind()
{
#if QT_CONFIG(opengl)
    QOpenGLContext *ctx = QOpenGLContext::currentContext();
    QOpenGLFunctions *funcs = ctx ? ctx->functions() : nullptr;
    if (!funcs)
        return;

    if (!textureId())
        return;

    funcs->glBindTexture(GL_TEXTURE_2D, m_textureId);

    if (m_uploaded)
        return;

    QByteArray logName(m_textureData ? m_textureData->logName : QByteArrayLiteral("(unset)"));

    if (!m_textureData || !m_textureData->isValid()) {
        qCDebug(QSG_LOG_TEXTUREIO, "Invalid texture data for %s", logName.constData());
        funcs->glBindTexture(GL_TEXTURE_2D, 0);
        return;
    }

    if (Q_UNLIKELY(QSG_LOG_TEXTUREIO().isDebugEnabled())) {
        qCDebug(QSG_LOG_TEXTUREIO) << "Uploading texture" << m_textureData.data();
        while (funcs->glGetError() != GL_NO_ERROR);
    }

    funcs->glCompressedTexImage2D(GL_TEXTURE_2D, 0, m_textureData->format,
                                  m_size.width(), m_size.height(), 0, m_textureData->sizeInBytes(),
                                  m_textureData->data.constData() + m_textureData->dataOffset);

    if (Q_UNLIKELY(QSG_LOG_TEXTUREIO().isDebugEnabled())) {
        GLuint error = funcs->glGetError();
        if (error != GL_NO_ERROR) {
            qCDebug(QSG_LOG_TEXTUREIO, "glCompressedTexImage2D failed for %s, error 0x%x", logName.constData(), error);
        }
    }

    m_textureData.clear();  // Release this memory, not needed anymore

    updateBindOptions(true);
    m_uploaded = true;
#endif // QT_CONFIG(opengl)
}

bool QSGCompressedTexture::formatIsOpaque(quint32 glTextureFormat)
{
    switch (glTextureFormat) {
    case QOpenGLTexture::RGB_DXT1:
    case QOpenGLTexture::R_ATI1N_UNorm:
    case QOpenGLTexture::R_ATI1N_SNorm:
    case QOpenGLTexture::RG_ATI2N_UNorm:
    case QOpenGLTexture::RG_ATI2N_SNorm:
    case QOpenGLTexture::RGB_BP_UNSIGNED_FLOAT:
    case QOpenGLTexture::RGB_BP_SIGNED_FLOAT:
    case QOpenGLTexture::R11_EAC_UNorm:
    case QOpenGLTexture::R11_EAC_SNorm:
    case QOpenGLTexture::RG11_EAC_UNorm:
    case QOpenGLTexture::RG11_EAC_SNorm:
    case QOpenGLTexture::RGB8_ETC2:
    case QOpenGLTexture::SRGB8_ETC2:
    case QOpenGLTexture::RGB8_ETC1:
    case QOpenGLTexture::SRGB_DXT1:
        return true;
        break;
    default:
        return false;
    }
}

QSGCompressedTextureFactory::QSGCompressedTextureFactory(const QSGCompressedTexture::DataPtr &texData)
    : m_textureData(texData)
{
}

QSGTexture *QSGCompressedTextureFactory::createTexture(QQuickWindow *window) const
{
    if (!m_textureData || !m_textureData->isValid())
        return nullptr;

    // attempt to atlas the texture
    QSGRenderContext *context = QQuickWindowPrivate::get(window)->context;
    QSGTexture *t = context->compressedTextureForFactory(this);
    if (t)
        return t;

    return new QSGCompressedTexture(m_textureData);
}

int QSGCompressedTextureFactory::textureByteCount() const
{
    return m_textureData ? m_textureData->sizeInBytes() : 0;
}


QSize QSGCompressedTextureFactory::textureSize() const
{
    if (m_textureData && m_textureData->isValid())
        return m_textureData->size;
    return QSize();
}

QT_END_NAMESPACE
