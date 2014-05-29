/* Webcamod, webcam capture plasmoid.
 * Copyright (C) 2011-2013  Gonzalo Exequiel Pedone
 *
 * Webcamod is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Webcamod is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Webcamod. If not, see <http://www.gnu.org/licenses/>.
 *
 * Email     : hipersayan DOT x AT gmail DOT com
 * Web-Site 1: http://github.com/hipersayanX/Webcamoid
 * Web-Site 2: http://kde-apps.org/content/show.php/Webcamoid?content=144796
 */

#ifndef PIPELINE_H
#define PIPELINE_H

#include <QtCore>
#include <qb.h>

typedef QMap<QString, QbElementPtr> ElementMap;
typedef QMap<QString, QThread *> ThreadsMap;

class Pipeline: public QObject
{
    Q_OBJECT

    Q_PROPERTY(ElementMap elements READ elements WRITE setElements RESET resetElements)
    Q_PROPERTY(QList<QStringList> links READ links WRITE setLinks RESET resetLinks)
    Q_PROPERTY(QList<QStringList> connections READ connections WRITE setConnections RESET resetConnections)
    Q_PROPERTY(QVariantMap properties READ properties WRITE setProperties RESET resetProperties)
    Q_PROPERTY(QString error READ error WRITE setError RESET resetError)
    Q_PROPERTY(QList<QbElementPtr> inputs READ inputs)
    Q_PROPERTY(QList<QbElementPtr> outputs READ outputs)
    Q_PROPERTY(ThreadsMap threads READ threads WRITE setThreads RESET resetThreads)

    public:
        explicit Pipeline(QObject *parent=NULL);
        Q_INVOKABLE QMap<QString, QbElementPtr> elements() const;
        Q_INVOKABLE QList<QStringList> links() const;
        Q_INVOKABLE QList<QStringList> connections() const;
        Q_INVOKABLE QVariantMap properties() const;
        Q_INVOKABLE QString error() const;
        Q_INVOKABLE QString addElement(QbElementPtr element);
        Q_INVOKABLE void removeElement(const QString &elementName);
        Q_INVOKABLE QList<QbElementPtr> inputs() const;
        Q_INVOKABLE QList<QbElementPtr> outputs() const;
        Q_INVOKABLE ThreadsMap threads() const;
        Q_INVOKABLE QList<Qt::ConnectionType> outputConnectionTypes() const;
        Q_INVOKABLE bool linkAll();
        Q_INVOKABLE bool unlinkAll();
        Q_INVOKABLE bool connectAll();
        Q_INVOKABLE bool disconnectAll();

    private:
        QMap<QString, QbElementPtr> m_elements;
        QList<QStringList> m_links;
        QList<QStringList> m_connections;
        QVariantMap m_properties;
        QString m_error;
        ThreadsMap m_threads;

        QMetaMethod methodByName(QObject *object, const QString &methodName, QMetaMethod::MethodType methodType);

    public slots:
        void solveConnections(QString self);
        void addLinks(const QStringList &links);
        void cleanAll();
        void setElements(const QMap<QString, QbElementPtr> &elements);
        void setLinks(const QList<QStringList> &links);
        void setConnections(const QList<QStringList> &connections);
        void setProperties(const QVariantMap &properties);
        void setError(const QString &error);
        void setThreads(const ThreadsMap &threads);
        void resetElements();
        void resetLinks();
        void resetConnections();
        void resetProperties();
        void resetError();
        void resetThreads();
};

#endif // PIPELINE_H
