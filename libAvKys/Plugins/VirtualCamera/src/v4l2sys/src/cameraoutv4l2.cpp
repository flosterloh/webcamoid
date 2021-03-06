/* Webcamoid, webcam capture application.
 * Copyright (C) 2011-2017  Gonzalo Exequiel Pedone
 *
 * Webcamoid is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Webcamoid is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Webcamoid. If not, see <http://www.gnu.org/licenses/>.
 *
 * Web-Site: http://webcamoid.github.io/
 */

#include <QDebug>
#include <QMap>
#include <QDir>
#include <QSize>
#include <QProcess>
#include <QFileSystemWatcher>
#include <akvideocaps.h>
#include <akvideopacket.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/videodev2.h>

#ifdef HAVE_V4LUTILS
#include <libv4l2.h>

#define x_ioctl v4l2_ioctl
#define x_open v4l2_open
#define x_close v4l2_close
#define x_write v4l2_write
#else
#include <unistd.h>
#include <sys/ioctl.h>

#define x_ioctl ioctl
#define x_open open
#define x_close close
#define x_write write
#endif

#include "cameraoutv4l2.h"

#define MAX_CAMERAS 64
#define LOOPBACK_DEVICE "v4l2loopback"

typedef QMap<AkVideoCaps::PixelFormat, quint32> V4l2PixFmtMap;

inline V4l2PixFmtMap initV4l2PixFmtMap()
{
    V4l2PixFmtMap ffToV4L2 = {
        // RGB formats
#ifdef V4L2_PIX_FMT_RGB444
        {AkVideoCaps::Format_rgb444le, V4L2_PIX_FMT_RGB444 },
#endif
        {AkVideoCaps::Format_rgb555le, V4L2_PIX_FMT_RGB555 },
        {AkVideoCaps::Format_rgb565le, V4L2_PIX_FMT_RGB565 },
        {AkVideoCaps::Format_rgb555be, V4L2_PIX_FMT_RGB555X},
        {AkVideoCaps::Format_rgb565be, V4L2_PIX_FMT_RGB565X},
        {AkVideoCaps::Format_bgr24   , V4L2_PIX_FMT_BGR24  },
        {AkVideoCaps::Format_rgb24   , V4L2_PIX_FMT_RGB24  },
        {AkVideoCaps::Format_bgr0    , V4L2_PIX_FMT_BGR32  },
        {AkVideoCaps::Format_0rgb    , V4L2_PIX_FMT_RGB32  },

        // Grey formats
        {AkVideoCaps::Format_gray    , V4L2_PIX_FMT_GREY},
#ifdef V4L2_PIX_FMT_Y16
        {AkVideoCaps::Format_gray16le, V4L2_PIX_FMT_Y16 },
#endif

        // Luminance+Chrominance formats
        {AkVideoCaps::Format_yuv410p, V4L2_PIX_FMT_YVU410 },
        {AkVideoCaps::Format_yuv420p, V4L2_PIX_FMT_YVU420 },
        {AkVideoCaps::Format_yuyv422, V4L2_PIX_FMT_YUYV   },
        {AkVideoCaps::Format_yuv422p, V4L2_PIX_FMT_YYUV   },
        {AkVideoCaps::Format_uyvy422, V4L2_PIX_FMT_UYVY   },
#ifdef V4L2_PIX_FMT_VYUY
        {AkVideoCaps::Format_yuv422p, V4L2_PIX_FMT_VYUY   },
#endif
        {AkVideoCaps::Format_yuv422p, V4L2_PIX_FMT_YUV422P},
        {AkVideoCaps::Format_yuv411p, V4L2_PIX_FMT_YUV411P},
        {AkVideoCaps::Format_yuv411p, V4L2_PIX_FMT_Y41P   },
        {AkVideoCaps::Format_yuv410p, V4L2_PIX_FMT_YUV410 },
        {AkVideoCaps::Format_yuv420p, V4L2_PIX_FMT_YUV420 },

        // two planes -- one Y, one Cr + Cb interleaved
        {AkVideoCaps::Format_nv12, V4L2_PIX_FMT_NV12},
        {AkVideoCaps::Format_nv21, V4L2_PIX_FMT_NV21},
#ifdef V4L2_PIX_FMT_NV16
        {AkVideoCaps::Format_nv16, V4L2_PIX_FMT_NV16},
#endif

        // Bayer formats
#ifdef V4L2_PIX_FMT_SBGGR8
        {AkVideoCaps::Format_bayer_bggr8, V4L2_PIX_FMT_SBGGR8},
#endif
#ifdef V4L2_PIX_FMT_SGBRG8
        {AkVideoCaps::Format_bayer_gbrg8, V4L2_PIX_FMT_SGBRG8},
#endif
#ifdef V4L2_PIX_FMT_SGRBG8
        {AkVideoCaps::Format_bayer_grbg8, V4L2_PIX_FMT_SGRBG8},
#endif
#ifdef V4L2_PIX_FMT_SRGGB8
        {AkVideoCaps::Format_bayer_rggb8, V4L2_PIX_FMT_SRGGB8},
#endif

        // 10bit raw bayer, expanded to 16 bits
#ifdef V4L2_PIX_FMT_SBGGR16
        {AkVideoCaps::Format_bayer_bggr16le, V4L2_PIX_FMT_SBGGR16},
#endif
    };

    return ffToV4L2;
}

Q_GLOBAL_STATIC_WITH_ARGS(V4l2PixFmtMap, ffToV4L2, (initV4l2PixFmtMap()))

class CameraOutV4L2Private
{
    public:
        CameraOutV4L2 *self;
        QStringList m_webcams;
        int m_streamIndex;
        QFileSystemWatcher *m_fsWatcher;
        QFile m_deviceFile;

        CameraOutV4L2Private(CameraOutV4L2 *self):
            self(self),
            m_streamIndex(1),
            m_fsWatcher(nullptr)
        {
        }

        inline int xioctl(int fd, ulong request, void *arg) const;
        inline QStringList availableMethods() const;
        inline bool isModuleLoaded() const;
        inline bool sudo(const QString &command,
                         const QStringList &argumments,
                         const QString &password) const;
        inline void rmmod(const QString &password) const;
        inline bool updateCameras(const QStringList &webcamIds,
                                  const QStringList &webcamDescriptions,
                                  const QString &password) const;
        inline QString cleanupDescription(const QString &description) const;
};

CameraOutV4L2::CameraOutV4L2(QObject *parent):
    CameraOut(parent)
{
    this->d = new CameraOutV4L2Private(this);
    auto methods = this->d->availableMethods();

    if (!methods.isEmpty())
        this->m_rootMethod = methods.first();

    this->d->m_webcams = this->webcams();
    this->d->m_fsWatcher = new QFileSystemWatcher(QStringList() << "/dev");
    this->d->m_fsWatcher->setParent(this);

    QObject::connect(this->d->m_fsWatcher,
                     &QFileSystemWatcher::directoryChanged,
                     this,
                     &CameraOutV4L2::onDirectoryChanged);
    QObject::connect(this,
                     &CameraOutV4L2::rootMethodChanged,
                     [this] () {
        emit this->needRootChanged(this->needRoot());
    });
}

CameraOutV4L2::~CameraOutV4L2()
{
    delete this->d->m_fsWatcher;
    delete this->d;
}

QStringList CameraOutV4L2::webcams() const
{
    QDir devicesDir("/dev");

    QStringList devices = devicesDir.entryList(QStringList() << "video*",
                                               QDir::System
                                               | QDir::Readable
                                               | QDir::Writable
                                               | QDir::NoSymLinks
                                               | QDir::NoDotAndDotDot
                                               | QDir::CaseSensitive,
                                               QDir::Name);

    QStringList webcams;
    QFile device;
    v4l2_capability capability;
    memset(&capability, 0, sizeof(v4l2_capability));

    for (const QString &devicePath: devices) {
        device.setFileName(devicesDir.absoluteFilePath(devicePath));

        if (device.open(QIODevice::ReadWrite)) {
            this->d->xioctl(device.handle(), VIDIOC_QUERYCAP, &capability);

            if (capability.capabilities & V4L2_CAP_VIDEO_OUTPUT)
                webcams << device.fileName();

            device.close();
        }
    }

    return webcams;
}

int CameraOutV4L2::streamIndex() const
{
    return this->d->m_streamIndex;
}

QString CameraOutV4L2::description(const QString &webcam) const
{
    if (webcam.isEmpty())
        return QString();

    QFile device;
    v4l2_capability capability;
    memset(&capability, 0, sizeof(v4l2_capability));

    device.setFileName(webcam);

    if (device.open(QIODevice::ReadWrite)) {
        this->d->xioctl(device.handle(), VIDIOC_QUERYCAP, &capability);

        if (capability.capabilities & V4L2_CAP_VIDEO_OUTPUT)
            return QString(reinterpret_cast<const char *>(capability.card));

        device.close();
    }

    return QString();
}

void CameraOutV4L2::writeFrame(const AkPacket &frame)
{
    if (!this->d->m_deviceFile.isOpen())
        return;

    if (this->d->m_deviceFile.write(frame.buffer()) < 0)
        qDebug() << "Error writing frame";
}

int CameraOutV4L2::maxCameras() const
{
    QString modules = QString("/lib/modules/%1/modules.dep")
                      .arg(QSysInfo::kernelVersion());

    QFile file(modules);

    if (!file.open(QIODevice::ReadOnly))
        return 0;

    forever {
        QByteArray line = file.readLine();

        if (line.isEmpty())
            break;

        QString module = QFileInfo(line.left(line.indexOf(':'))).baseName();

        if (module == LOOPBACK_DEVICE) {
            file.close();

            return MAX_CAMERAS;
        }
    }

    file.close();

    return 0;
}

bool CameraOutV4L2::needRoot() const
{
    return this->m_rootMethod == "su" || this->m_rootMethod == "sudo";
}

QString CameraOutV4L2::createWebcam(const QString &description,
                                    const QString &password)
{
    if ((this->m_rootMethod == "su" || this->m_rootMethod == "sudo")
        && password.isEmpty())
        return QString();

    QStringList webcams = this->webcams();
    QStringList webcamDescriptions;
    QStringList webcamIds;

    for (const QString &webcam: webcams) {
        webcamDescriptions << this->description(webcam);
        int id = webcam.indexOf(QRegExp("[0-9]+"));
        webcamIds << webcam.mid(id);
    }

    int id = 0;

    for (; QFileInfo::exists(QString("/dev/video%1").arg(id)); id++) {
    }

    QString deviceDescription;

    if (description.isEmpty())
        deviceDescription = QString(tr("Virtual Camera %1")).arg(id);
    else
        deviceDescription = this->d->cleanupDescription(description);

    webcamDescriptions << deviceDescription;
    webcamIds << QString("%1").arg(id);

    if (!this->d->updateCameras(webcamIds, webcamDescriptions, password))
        return QString();

    QStringList curWebcams = this->webcams();

    if (curWebcams != webcams)
        emit this->webcamsChanged(curWebcams);

    return QString("/dev/video%1").arg(id);
}

bool CameraOutV4L2::changeDescription(const QString &webcam,
                                      const QString &description,
                                      const QString &password)
{
    if ((this->m_rootMethod == "su" || this->m_rootMethod == "sudo")
        && password.isEmpty())
        return false;

    if (!QRegExp("/dev/video[0-9]+").exactMatch(webcam))
        return false;

    QStringList webcams = this->webcams();

    if (webcams.isEmpty()
        || !webcams.contains(webcam))
        return false;

    QStringList webcamDescriptions;
    QStringList webcamIds;

    for (const QString &webcam: webcams) {
        webcamDescriptions << this->description(webcam);
        int id = webcam.indexOf(QRegExp("[0-9]+"));
        webcamIds << webcam.mid(id);
    }

    int id = webcam.indexOf(QRegExp("[0-9]+"));
    bool ok = false;
    id = webcam.mid(id).toInt(&ok);

    if (!ok)
        return false;

    QString deviceDescription;

    if (description.isEmpty())
        deviceDescription = QString(tr("Virtual Camera %1")).arg(id);
    else
        deviceDescription = this->d->cleanupDescription(description);

    int index = webcamIds.indexOf(QString("%1").arg(id));

    if (index < 0)
        return false;

    webcamDescriptions[index] = deviceDescription;

    if (!this->d->updateCameras(webcamIds, webcamDescriptions, password))
        return false;

    QStringList curWebcams = this->webcams();

    if (curWebcams != webcams)
        emit this->webcamsChanged(curWebcams);

    return true;
}

bool CameraOutV4L2::removeWebcam(const QString &webcam,
                                 const QString &password)
{
    if ((this->m_rootMethod == "su" || this->m_rootMethod == "sudo")
        && password.isEmpty())
        return false;

    if (!QRegExp("/dev/video[0-9]+").exactMatch(webcam))
        return false;

    QStringList webcams = this->webcams();

    if (webcams.isEmpty()
        || !webcams.contains(webcam))
        return false;

    QStringList webcamDescriptions;
    QStringList webcamIds;

    for (const QString &webcam: webcams) {
        webcamDescriptions << this->description(webcam);
        int id = webcam.indexOf(QRegExp("[0-9]+"));
        webcamIds << webcam.mid(id);
    }

    int id = webcam.indexOf(QRegExp("[0-9]+"));
    bool ok = false;
    id = webcam.mid(id).toInt(&ok);

    if (!ok)
        return false;

    int index = webcamIds.indexOf(QString("%1").arg(id));

    if (index < 0)
        return false;

    webcamDescriptions.removeAt(index);
    webcamIds.removeAt(index);

    if (!this->d->updateCameras(webcamIds, webcamDescriptions, password))
        return false;

    QStringList curWebcams = this->webcams();

    if (curWebcams != webcams)
        emit this->webcamsChanged(curWebcams);

    return true;
}

bool CameraOutV4L2::removeAllWebcams(const QString &password)
{
    if ((this->m_rootMethod == "su" || this->m_rootMethod == "sudo")
        && password.isEmpty())
        return false;

    QStringList webcams = this->webcams();

    if (webcams.isEmpty())
        return false;

    this->d->rmmod(password);
    QStringList curWebcams = this->webcams();

    if (curWebcams != webcams)
        emit this->webcamsChanged(curWebcams);

    return true;
}

int CameraOutV4L2Private::xioctl(int fd, ulong request, void *arg) const
{
    int r = -1;

    forever {
        r = x_ioctl(fd, request, arg);

        if (r != -1 || errno != EINTR)
            break;
    }

    return r;
}

/* List of possible graphical sudo methods that can be supported:
 *
 * https://en.wikipedia.org/wiki/Comparison_of_privilege_authorization_features#Introduction_to_implementations
 *
 * NOTE: 'su' and 'sudo' methods must be removed.
 */
QStringList CameraOutV4L2Private::availableMethods() const
{
    auto paths = QProcessEnvironment::systemEnvironment().value("PATH").split(':');

    QStringList sus {
        "gksu",
        "gksudo",
        "gtksu",
        "kdesu",
        "kdesudo",
        "su",
        "sudo"
    };

    QStringList methods;

    for (auto &su: sus)
        for (auto &path: paths)
            if (QDir(path).exists(su)) {
                methods << su;

                break;
            }

    return methods;
}

bool CameraOutV4L2Private::isModuleLoaded() const
{
    QProcess lsmod;
    lsmod.start("lsmod");
    lsmod.waitForFinished();

    // If for whatever reason the command failed to execute, we will assume
    // that the module is loaded.
    if (lsmod.exitCode() != 0)
        return true;

    for (const QByteArray &line: lsmod.readAllStandardOutput().split('\n'))
        if (line.trimmed().startsWith(LOOPBACK_DEVICE))
            return true;

    return false;
}

bool CameraOutV4L2Private::sudo(const QString &command,
                                const QStringList &argumments,
                                const QString &password) const
{
    QProcess su;

    if (self->rootMethod() == "su"
        || self->rootMethod() == "sudo") {
        if (password.isEmpty())
            return false;

        QProcess echo;
        echo.setStandardOutputProcess(&su);

        if (self->rootMethod() == "su") {
            QStringList args;

            for (QString arg: argumments)
                args << arg.replace(" ", "\\ ");

            echo.start("echo", {password});
            su.start("su", {"-c", command + " " + args.join(" ")});
        } else {
            echo.start("echo", {password});
            su.start("sudo", QStringList {"-S", command} << argumments);
        }

        su.setProcessChannelMode(QProcess::ForwardedChannels);
        echo.waitForStarted();

        if (!su.waitForFinished(self->passwordTimeout())) {
            su.kill();
            echo.waitForFinished();

            return false;
        }

        echo.waitForFinished();
    } else {
        su.start(self->rootMethod(), QStringList {command} << argumments);
        su.waitForFinished(-1);
    }

    if (su.exitCode()) {
        QByteArray outMsg = su.readAllStandardOutput();

        if (!outMsg.isEmpty())
            qDebug() << outMsg.toStdString().c_str();

        QByteArray errorMsg = su.readAllStandardError();

        if (!errorMsg.isEmpty())
            qDebug() << errorMsg.toStdString().c_str();

        return false;
    }

    return true;
}

void CameraOutV4L2Private::rmmod(const QString &password) const
{
    if (this->isModuleLoaded())
        this->sudo("rmmod", {LOOPBACK_DEVICE}, password);
}

bool CameraOutV4L2Private::updateCameras(const QStringList &webcamIds,
                                         const QStringList &webcamDescriptions,
                                         const QString &password) const
{
    if (this->isModuleLoaded()) {
        if (!this->sudo("sh",
                        {"-c",
                         QString("rmmod %1; "
                                 "modprobe %1 "
                                 "video_nr=%2 "
                                 "'card_label=%3'").arg(LOOPBACK_DEVICE)
                                                   .arg(webcamIds.join(','))
                                                   .arg(webcamDescriptions.join(','))},
                        password))
            return false;
    } else {
        if (!webcamIds.isEmpty()) {
            if (!this->sudo("modprobe",
                            {LOOPBACK_DEVICE,
                             QString("video_nr=%1").arg(webcamIds.join(',')),
                             QString("card_label=%1").arg(webcamDescriptions.join(','))},
                            password))
                return false;
        }
    }

    return true;
}

QString CameraOutV4L2Private::cleanupDescription(const QString &description) const
{
    QString cleanDescription;

    for (auto &c: description)
        cleanDescription.append(c.isSymbol() || c.isSpace()?
                                    QString("\\%1").arg(c): c);

    return description;
}

bool CameraOutV4L2::init(int streamIndex)
{
    if (!this->m_caps)
        return false;

    this->d->m_deviceFile.setFileName(this->m_device);

    if (!this->d->m_deviceFile.open(QIODevice::WriteOnly)) {
        emit this->error(QString("Unable to open V4L2 device %1").arg(this->m_device));

        return false;
    }

    if (fcntl(this->d->m_deviceFile.handle(), F_SETFL, O_NONBLOCK) < 0) {
        emit this->error(QString("Can't set V4L2 device %1 in blocking mode").arg(this->m_device));

        return false;
    }

    v4l2_format fmt;
    memset(&fmt, 0, sizeof(v4l2_format));
    fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

    if (this->d->xioctl(this->d->m_deviceFile.handle(), VIDIOC_G_FMT, &fmt) < 0) {
        emit this->error("Can't read default format");
        this->d->m_deviceFile.close();

        return false;
    }

    AkVideoCaps videoCaps(this->m_caps);
    fmt.fmt.pix.width = __u32(videoCaps.width());
    fmt.fmt.pix.height = __u32(videoCaps.height());
    fmt.fmt.pix.pixelformat = ffToV4L2->value(videoCaps.format());
    fmt.fmt.pix.sizeimage = __u32(videoCaps.pictureSize());

    if (this->d->xioctl(this->d->m_deviceFile.handle(),
                        VIDIOC_S_FMT,
                        &fmt) < 0) {
        emit this->error("Can't set format");
        this->d->m_deviceFile.close();

        return false;
    }

    this->d->m_streamIndex = streamIndex;

    return true;
}

void CameraOutV4L2::uninit()
{
    this->d->m_deviceFile.close();
}

void CameraOutV4L2::resetRootMethod()
{
    auto methods = this->d->availableMethods();

    if (methods.isEmpty())
        this->setRootMethod("");
    else
        this->setRootMethod(methods.first());
}

void CameraOutV4L2::onDirectoryChanged(const QString &path)
{
    Q_UNUSED(path)

    QStringList webcams = this->webcams();

    if (webcams != this->d->m_webcams) {
        emit this->webcamsChanged(webcams);

        this->d->m_webcams = webcams;
    }
}

#include "moc_cameraoutv4l2.cpp"
