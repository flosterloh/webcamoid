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

#ifndef OUTPUTPARAMS_H
#define OUTPUTPARAMS_H

#include <qb.h>
#include "customdeleters.h"

typedef QSharedPointer<AVCodecContext> CodecContextPtr;

class OutputParams: public QObject
{
    Q_OBJECT
    Q_PROPERTY(CodecContextPtr codecContext READ codecContext WRITE setCodecContext RESET resetCodecContext)
    Q_PROPERTY(QbElementPtr filter READ filter WRITE setFilter RESET resetFilter)
    Q_PROPERTY(int outputIndex READ outputIndex WRITE setOutputIndex RESET resetOutputIndex)
    Q_PROPERTY(qint64 pts READ pts WRITE setPts RESET resetPts)
    Q_PROPERTY(int duration READ duration WRITE setDuration RESET resetDuration)

    public:
        explicit OutputParams(QObject *parent=NULL);

        OutputParams(CodecContextPtr codecContext,
                     QbElementPtr filter,
                     int outputIndex,
                     qint64 pts=0,
                     int duration=0);

        OutputParams(const OutputParams &other);
        OutputParams &operator =(const OutputParams &other);

        Q_INVOKABLE CodecContextPtr codecContext() const;
        Q_INVOKABLE QbElementPtr filter() const;
        Q_INVOKABLE int outputIndex() const;
        Q_INVOKABLE qint64 pts() const;
        Q_INVOKABLE int duration() const;

    private:
        CodecContextPtr m_codecContext;
        QbElementPtr m_filter;
        int m_outputIndex;
        qint64 m_pts;
        int m_duration;

    public slots:
        void setCodecContext(CodecContextPtr codecContext);
        void setFilter(QbElementPtr filter);
        void setOutputIndex(int outputIndex);
        void setPts(qint64 pts);
        void setDuration(int duration);
        void resetCodecContext();
        void resetFilter();
        void resetOutputIndex();
        void resetPts();
        void resetDuration();
        void increasePts();
};

Q_DECLARE_METATYPE(OutputParams)

#endif // OUTPUTPARAMS_H
