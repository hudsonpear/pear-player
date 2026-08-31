#include "MpvPlayer.h"
#include "Equalizer.h"

#include <QStringList>
#include <QUrl>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QOpenGLContext>
#include <QMetaObject>
#include <QByteArray>
#include <QDebug>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <initializer_list>
#include <vector>

namespace {

// Seek-back distance mpv keeps for a stream it cannot seek in. Only network
// streams get the large one: see loadFile().
constexpr const char *kNetworkStreamBufferSize = "512MiB";
constexpr const char *kLocalStreamBufferSize = "128KiB"; // mpv's own default

// libmpv commands take a NULL-terminated argv of C strings. Returns mpv's
// status: negative on failure, which matters for commands that can fail for
// reasons worth telling the user about (an unwritable screenshot path).
int mpvCommand(mpv_handle *mpv, std::initializer_list<const char *> args)
{
    std::vector<const char *> argv(args);
    argv.push_back(nullptr);
    return mpv_command(mpv, argv.data());
}

} // namespace

MpvPlayer::MpvPlayer(QObject *parent)
    : QObject(parent)
{
}

MpvPlayer::~MpvPlayer()
{
    releaseRender();
    if (mpv_) {
        mpv_terminate_destroy(mpv_);
        mpv_ = nullptr;
    }
}

void MpvPlayer::initialize(bool hwDecEnabled)
{
    mpv_ = mpv_create();
    if (!mpv_) {
        emit errorOccurred(QStringLiteral("Failed to create mpv core"));
        return;
    }

    // Keep mpv self-contained: no user mpv.conf, no OSD/OSC (Qt draws the
    // controls), no built-in input handling (Qt forwards key/mouse events).
    mpv_set_option_string(mpv_, "config", "no");
    mpv_set_option_string(mpv_, "terminal", "no");
    mpv_set_option_string(mpv_, "osc", "no");
    mpv_set_option_string(mpv_, "input-default-bindings", "no");
    mpv_set_option_string(mpv_, "input-vo-keyboard", "no");
    mpv_set_option_string(mpv_, "input-cursor", "no");

    mpv_set_option_string(mpv_, "vo", "libmpv");
    mpv_set_option_string(mpv_, "gpu-api", "opengl");
    // hwdec=auto-safe corrupts frames (horizontal colored lines) on some
    // GPU/driver combinations, so software decoding ("no") is the safe
    // default; the Settings dialog lets users opt back into hwdec.
    mpv_set_option_string(mpv_, "hwdec", hwDecEnabled ? "auto-safe" : "no");
    mpv_set_option_string(mpv_, "keep-open", "yes");
    // A still would otherwise close itself after mpv's default second. This
    // affects images only, so it is set once rather than per file.
    mpv_set_option_string(mpv_, "image-display-duration", "inf");
    mpv_set_option_string(mpv_, "keepaspect", "yes");
    // mpv's default ("exact") only picks up a subtitle named exactly like the
    // video -- movie.srt for movie.mp4. Downloads almost never arrive that way:
    // they carry a language or release tag (movie.en.srt, movie.1080p.srt),
    // which "fuzzy" matches because it takes any subtitle whose name contains
    // the video's.
    mpv_set_option_string(mpv_, "sub-auto", "fuzzy");
    // mpv's default volume-max is 130; raise it so setVolume() can actually
    // amplify up to 200% instead of silently clamping at 130.
    mpv_set_option_string(mpv_, "volume-max", "200");

    // An .mp4 keeps its index (the "moov" atom) at the end of the file unless
    // it was explicitly prepared for streaming, so the demuxer has to read to
    // the end and then seek back to the header. Over HTTP that needs range
    // requests, and servers that ignore them leave mpv with a strictly linear
    // stream: the load fails with "Cannot seek backward in linear streams".
    // The same file plays in a browser, which just buffers the whole thing.
    //
    // kNetworkStreamBufferSize is the maximum seek-back distance that case
    // needs, and loadFile() switches it in only for network streams: setting
    // it globally made every local file take about 700ms longer to open.

    if (const int status = mpv_initialize(mpv_); status < 0) {
        emit errorOccurred(QStringLiteral("Failed to initialize mpv: %1").arg(mpv_error_string(status)));
        return;
    }

    // Error-level only: enough to explain a failed load without the volume
    // (or the performance cost) of mpv's verbose logging.
    mpv_request_log_messages(mpv_, "error");

    observeProperties();
    mpv_set_wakeup_callback(mpv_, &MpvPlayer::onMpvWakeup, this);
}

void MpvPlayer::observeProperties()
{
    mpv_observe_property(mpv_, 0, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv_, 0, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv_, 0, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(mpv_, 0, "volume", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv_, 0, "mute", MPV_FORMAT_FLAG);
    mpv_observe_property(mpv_, 0, "speed", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv_, 0, "filename", MPV_FORMAT_STRING);
    mpv_observe_property(mpv_, 0, "media-title", MPV_FORMAT_STRING);
    // The end of playback arrives here, not as MPV_EVENT_END_FILE: with
    // keep-open=yes (set in initialize(), so a finished file stays on screen
    // instead of closing) mpv never ends the file at all -- it pauses and
    // sets this flag. Verified against libmpv: END_FILE simply never comes.
    mpv_observe_property(mpv_, 0, "eof-reached", MPV_FORMAT_FLAG);
    mpv_observe_property(mpv_, 0, "playlist-pos", MPV_FORMAT_INT64);
    mpv_observe_property(mpv_, 0, "playlist-count", MPV_FORMAT_INT64);
    // Empty (or unset) once the current file has no active video track --
    // the standard way to detect audio-only media.
    mpv_observe_property(mpv_, 0, "video-codec", MPV_FORMAT_STRING);
    // The size the picture is actually shown at. Observed rather than read on
    // FILE_LOADED because mpv fills it in once the video output has been
    // configured, which happens after the file reports itself loaded.
    mpv_observe_property(mpv_, 0, "dwidth", MPV_FORMAT_INT64);
    // Changes as playback crosses a chapter boundary as well as when one is
    // picked from the menu, so watching the property covers both.
    mpv_observe_property(mpv_, 0, "chapter", MPV_FORMAT_INT64);
}

void MpvPlayer::initRender()
{
    if (!mpv_ || renderCtx_) {
        return;
    }

    mpv_opengl_init_params glInitParams{};
    glInitParams.get_proc_address = &MpvPlayer::getProcAddress;
    glInitParams.get_proc_address_ctx = nullptr;

    // Deliberately not using MPV_RENDER_PARAM_ADVANCED_CONTROL: it requires
    // calling mpv_render_context_report_swap() after every render() or mpv
    // stalls presenting new frames. Basic mode needs none of that.
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL)},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &glInitParams},
        {MPV_RENDER_PARAM_INVALID, nullptr},
    };

    if (const int status = mpv_render_context_create(&renderCtx_, mpv_, params); status < 0) {
        emit errorOccurred(QStringLiteral("Failed to create mpv render context: %1").arg(mpv_error_string(status)));
        renderCtx_ = nullptr;
        return;
    }

    mpv_render_context_set_update_callback(renderCtx_, &MpvPlayer::onMpvRenderUpdate, this);
}

void MpvPlayer::render(int fbo, int width, int height)
{
    if (!renderCtx_) {
        return;
    }

    mpv_opengl_fbo mpvFbo{};
    mpvFbo.fbo = fbo;
    mpvFbo.w = width;
    mpvFbo.h = height;

    int flipY = 1;

    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_OPENGL_FBO, &mpvFbo},
        {MPV_RENDER_PARAM_FLIP_Y, &flipY},
        {MPV_RENDER_PARAM_INVALID, nullptr},
    };

    mpv_render_context_render(renderCtx_, params);
}

void MpvPlayer::releaseRender()
{
    if (renderCtx_) {
        mpv_render_context_free(renderCtx_);
        renderCtx_ = nullptr;
    }
}

void *MpvPlayer::getProcAddress(void *ctx, const char *name)
{
    Q_UNUSED(ctx);
    QOpenGLContext *glContext = QOpenGLContext::currentContext();
    if (!glContext) {
        return nullptr;
    }
    return reinterpret_cast<void *>(glContext->getProcAddress(QByteArray(name)));
}

void MpvPlayer::onMpvWakeup(void *ctx)
{
    auto *self = static_cast<MpvPlayer *>(ctx);
    QMetaObject::invokeMethod(self, &MpvPlayer::processMpvEvents, Qt::QueuedConnection);
}

void MpvPlayer::onMpvRenderUpdate(void *ctx)
{
    auto *self = static_cast<MpvPlayer *>(ctx);
    QMetaObject::invokeMethod(self, &MpvPlayer::frameReady, Qt::QueuedConnection);
}

void MpvPlayer::processMpvEvents()
{
    if (!mpv_) {
        return;
    }
    while (true) {
        mpv_event *event = mpv_wait_event(mpv_, 0.0);
        if (event->event_id == MPV_EVENT_NONE) {
            break;
        }
        handleEvent(event);
    }
}

void MpvPlayer::handleEvent(mpv_event *event)
{
    switch (event->event_id) {
    case MPV_EVENT_PROPERTY_CHANGE:
        handlePropertyChange(static_cast<mpv_event_property *>(event->data));
        break;

    case MPV_EVENT_FILE_LOADED: {
        char *filename = mpv_get_property_string(mpv_, "filename");
        if (filename) {
            emit fileLoaded(QString::fromUtf8(filename));
            mpv_free(filename);
        }

        // Authoritative check, scoped to "a file just loaded": the
        // video-codec *property-change* stream alone can't tell "no file
        // open yet" apart from "this file has no video track" -- both
        // report as MPV_FORMAT_NONE, which caused the note to show at
        // startup before anything was even loaded.
        char *videoCodec = mpv_get_property_string(mpv_, "video-codec");
        emit hasVideoChanged(videoCodec != nullptr && videoCodec[0] != '\0');
        if (videoCodec) {
            mpv_free(videoCodec);
        }
        break;
    }

    case MPV_EVENT_END_FILE: {
        auto *endFile = static_cast<mpv_event_end_file *>(event->data);
        if (endFile->reason == MPV_END_FILE_REASON_EOF) {
            emit playbackFinished();
        } else if (endFile->reason == MPV_END_FILE_REASON_ERROR) {
            // A file or URL that never opened ends here rather than at EOF.
            // Without this the failure is silent: playback simply never
            // starts and nothing tells the user why.
            const QString detail = firstLogError_.isEmpty()
                ? QString::fromUtf8(mpv_error_string(endFile->error))
                : firstLogError_;
            emit errorOccurred(QStringLiteral("Could not play this media: %1").arg(detail));
        }
        break;
    }

    case MPV_EVENT_LOG_MESSAGE: {
        // Only error-level lines arrive here (see mpv_request_log_messages).
        // Kept, not emitted: it becomes the detail on the next failed load.
        auto *logMessage = static_cast<mpv_event_log_message *>(event->data);
        const QString text = QString::fromUtf8(logMessage->text).trimmed();
        if (!text.isEmpty() && firstLogError_.isEmpty()) {
            firstLogError_ = text;
        }
        break;
    }

    case MPV_EVENT_START_FILE:
    case MPV_EVENT_SHUTDOWN:
    default:
        break;
    }
}

void MpvPlayer::handlePropertyChange(mpv_event_property *prop)
{
    const QLatin1StringView name(prop->name);

    if (name == QLatin1StringView("time-pos")) {
        if (prop->format == MPV_FORMAT_DOUBLE) {
            position_ = *static_cast<double *>(prop->data);
            emit positionChanged(position_);
        }
    } else if (name == QLatin1StringView("duration")) {
        if (prop->format == MPV_FORMAT_DOUBLE) {
            duration_ = *static_cast<double *>(prop->data);
            emit durationChanged(duration_);
        }
    } else if (name == QLatin1StringView("pause")) {
        if (prop->format == MPV_FORMAT_FLAG) {
            paused_ = *static_cast<int *>(prop->data) != 0;
            emit pauseChanged(paused_);
        }
    } else if (name == QLatin1StringView("volume")) {
        if (prop->format == MPV_FORMAT_DOUBLE) {
            volume_ = static_cast<int>(std::lround(*static_cast<double *>(prop->data)));
            emit volumeChanged(volume_);
        }
    } else if (name == QLatin1StringView("eof-reached")) {
        if (prop->format == MPV_FORMAT_FLAG) {
            const bool atEnd = *static_cast<int *>(prop->data) != 0;
            // Only the transition into EOF counts as "finished"; mpv clears
            // the flag again on the next load and on seeking back.
            if (atEnd && !eofReached_) {
                emit playbackFinished();
            }
            if (atEnd != eofReached_) {
                emit endOfFileChanged(atEnd);
            }
            eofReached_ = atEnd;
        }
    } else if (name == QLatin1StringView("mute")) {
        if (prop->format == MPV_FORMAT_FLAG) {
            muted_ = *static_cast<int *>(prop->data) != 0;
            emit muteChanged(muted_);
        }
    } else if (name == QLatin1StringView("speed")) {
        if (prop->format == MPV_FORMAT_DOUBLE) {
            speed_ = *static_cast<double *>(prop->data);
            emit speedChanged(speed_);
        }
    } else if (name == QLatin1StringView("media-title")) {
        if (prop->format == MPV_FORMAT_STRING) {
            emit mediaTitleChanged(QString::fromUtf8(*static_cast<char **>(prop->data)));
        }
    } else if (name == QLatin1StringView("video-codec")) {
        // MPV_FORMAT_NONE means "no file open yet", not "no video track" --
        // that case is already handled authoritatively in the
        // MPV_EVENT_FILE_LOADED handler. Only act on real string values
        // here (covers a video track appearing/disappearing mid-playback).
        if (prop->format == MPV_FORMAT_STRING) {
            const char *codec = *static_cast<char **>(prop->data);
            emit hasVideoChanged(codec != nullptr && codec[0] != '\0');
        }
    } else if (name == QLatin1StringView("chapter")) {
        if (prop->format == MPV_FORMAT_INT64) {
            emit chapterChanged(static_cast<int>(*static_cast<int64_t *>(prop->data)));
        }
    } else if (name == QLatin1StringView("dwidth")) {
        // dheight lands at the same time, so one of the pair is enough to
        // trigger on; both are read back together below.
        const QSize size = videoDisplaySize();
        if (!size.isEmpty()) {
            emit videoDisplaySizeChanged(size);
        }
    }
    // playlist-pos / playlist-count / filename are observed for future
    // playlist UI but are not surfaced as signals yet.
}

void MpvPlayer::loadFile(const QString &path)
{
    if (!mpv_) {
        return;
    }
    if (path.isEmpty()) {
        // mpv treats an empty filename as a load failure and reports it like
        // any other, so callers that mean "nothing" must use unload().
        return;
    }
    firstLogError_.clear();

    // The huge seek-back buffer is only needed for streams a server will not
    // let us seek in; see the comment in initialize(). Applying it to local
    // files as well cost roughly 700ms on every load -- measured at 835ms with
    // it against 93ms without -- so it is switched on per file instead.
    //
    // A bare Windows path parses as a URL with a single-letter scheme ("C"),
    // hence the length check rather than just asking whether a scheme exists.
    const QString scheme = QUrl(path).scheme();
    const bool isNetworkStream = scheme.size() > 1 && !QFileInfo::exists(path);
    mpv_set_property_string(mpv_, "stream-buffer-size",
                             isNetworkStream ? kNetworkStreamBufferSize : kLocalStreamBufferSize);
    const QByteArray utf8Path = path.toUtf8();
    mpvCommand(mpv_, {"loadfile", utf8Path.constData(), "replace"});
}

void MpvPlayer::unload()
{
    if (!mpv_) {
        return;
    }
    // mpv's own stop command, unlike stop() above: it closes the file and
    // empties the playlist, which is what releases the handle on disk.
    firstLogError_.clear();
    mpvCommand(mpv_, {"stop"});
}

void MpvPlayer::play()
{
    if (!mpv_) {
        return;
    }

    // With keep-open=yes, mpv parks on the last frame at end-of-file and
    // *stays* stopped even after pause is cleared; it only resumes once a
    // seek happens. Rewind first so "play" after the end restarts playback
    // instead of doing nothing.
    int eof = 0;
    mpv_get_property(mpv_, "eof-reached", MPV_FORMAT_FLAG, &eof);
    if (eof) {
        mpvCommand(mpv_, {"seek", "0", "absolute"});
    }

    int flag = 0;
    mpv_set_property(mpv_, "pause", MPV_FORMAT_FLAG, &flag);
}

void MpvPlayer::pause()
{
    if (!mpv_) {
        return;
    }
    int flag = 1;
    mpv_set_property(mpv_, "pause", MPV_FORMAT_FLAG, &flag);
}

void MpvPlayer::togglePause()
{
    if (!mpv_) {
        return;
    }
    if (paused_) {
        play();
    } else {
        pause();
    }
}

void MpvPlayer::stop()
{
    // "Stop" here means pause and rewind, not mpv's own stop command (which
    // unloads the file entirely and would require reopening it).
    if (!mpv_) {
        return;
    }
    int flag = 1;
    mpv_set_property(mpv_, "pause", MPV_FORMAT_FLAG, &flag);
    mpvCommand(mpv_, {"seek", "0", "absolute"});
}

void MpvPlayer::seekAbsolute(double seconds)
{
    if (!mpv_) {
        return;
    }
    const QByteArray value = QByteArray::number(seconds, 'f', 3);
    mpvCommand(mpv_, {"seek", value.constData(), "absolute"});
}

void MpvPlayer::seekRelative(double secondsDelta)
{
    if (!mpv_) {
        return;
    }
    const QByteArray value = QByteArray::number(secondsDelta, 'f', 3);
    mpvCommand(mpv_, {"seek", value.constData(), "relative"});
}

void MpvPlayer::frameStep()
{
    if (!mpv_) {
        return;
    }
    mpvCommand(mpv_, {"frame-step"});
}

void MpvPlayer::frameBackStep()
{
    if (!mpv_) {
        return;
    }
    mpvCommand(mpv_, {"frame-back-step"});
}

void MpvPlayer::playlistNext()
{
    if (!mpv_) {
        return;
    }
    mpvCommand(mpv_, {"playlist-next", "weak"});
}

void MpvPlayer::playlistPrevious()
{
    if (!mpv_) {
        return;
    }
    mpvCommand(mpv_, {"playlist-prev", "weak"});
}

void MpvPlayer::setSpeed(double speed)
{
    if (!mpv_) {
        return;
    }
    mpv_set_property(mpv_, "speed", MPV_FORMAT_DOUBLE, &speed);
}

void MpvPlayer::setLoop(bool loop)
{
    if (!mpv_) {
        return;
    }
    mpv_set_property_string(mpv_, "loop-file", loop ? "inf" : "no");
}

void MpvPlayer::setVolume(int volume0to200)
{
    if (!mpv_) {
        return;
    }
    double value = std::clamp(volume0to200, 0, maxVolume_);
    mpv_set_property(mpv_, "volume", MPV_FORMAT_DOUBLE, &value);
}

void MpvPlayer::setMaxVolume(int maxVolume)
{
    maxVolume_ = std::clamp(maxVolume, 100, 1000);
    if (!mpv_) {
        return;
    }
    mpv_set_property_string(mpv_, "volume-max", QByteArray::number(maxVolume_).constData());
    if (volume_ > maxVolume_) {
        setVolume(maxVolume_);
    }
}

namespace {

QVector<MpvPlayer::TrackInfo> tracksOfType(mpv_handle *mpv, QLatin1StringView type)
{
    QVector<MpvPlayer::TrackInfo> tracks;
    if (!mpv) {
        return tracks;
    }

    mpv_node node;
    if (mpv_get_property(mpv, "track-list", MPV_FORMAT_NODE, &node) < 0) {
        return tracks;
    }

    if (node.format == MPV_FORMAT_NODE_ARRAY) {
        for (int i = 0; i < node.u.list->num; ++i) {
            const mpv_node &entry = node.u.list->values[i];
            if (entry.format != MPV_FORMAT_NODE_MAP) {
                continue;
            }
            const mpv_node_list *map = entry.u.list;
            MpvPlayer::TrackInfo track;
            bool matchesType = false;
            for (int k = 0; k < map->num; ++k) {
                const QLatin1StringView key(map->keys[k]);
                const mpv_node &value = map->values[k];
                if (key == QLatin1StringView("type") && value.format == MPV_FORMAT_STRING) {
                    matchesType = QLatin1StringView(value.u.string) == type;
                } else if (key == QLatin1StringView("id") && value.format == MPV_FORMAT_INT64) {
                    track.id = static_cast<int>(value.u.int64);
                } else if (key == QLatin1StringView("title") && value.format == MPV_FORMAT_STRING) {
                    track.title = QString::fromUtf8(value.u.string);
                } else if (key == QLatin1StringView("lang") && value.format == MPV_FORMAT_STRING) {
                    track.language = QString::fromUtf8(value.u.string);
                } else if (key == QLatin1StringView("selected") && value.format == MPV_FORMAT_FLAG) {
                    track.selected = value.u.flag != 0;
                } else if (key == QLatin1StringView("external") && value.format == MPV_FORMAT_FLAG) {
                    track.external = value.u.flag != 0;
                }
            }
            if (matchesType) {
                tracks.append(track);
            }
        }
    }

    mpv_free_node_contents(&node);
    return tracks;
}

void selectTrack(mpv_handle *mpv, const char *property, int trackId)
{
    if (!mpv) {
        return;
    }
    if (trackId <= 0) {
        mpv_set_property_string(mpv, property, "no");
    } else {
        mpv_set_property_string(mpv, property, QByteArray::number(trackId).constData());
    }
}

} // namespace

QVector<MpvPlayer::TrackInfo> MpvPlayer::subtitleTracks() const
{
    return tracksOfType(mpv_, QLatin1StringView("sub"));
}

void MpvPlayer::setSubtitleTrack(int trackId)
{
    selectTrack(mpv_, "sid", trackId);
}

void MpvPlayer::loadExternalSubtitle(const QString &path)
{
    if (!mpv_) {
        return;
    }
    const QByteArray utf8Path = path.toUtf8();
    mpvCommand(mpv_, {"sub-add", utf8Path.constData(), "select"});
}

namespace {

void setSubProperty(mpv_handle *mpv, const char *name, const QString &value)
{
    if (mpv) {
        mpv_set_property_string(mpv, name, value.toUtf8().constData());
    }
}

void setSubProperty(mpv_handle *mpv, const char *name, double value)
{
    if (mpv) {
        mpv_set_property(mpv, name, MPV_FORMAT_DOUBLE, &value);
    }
}

void setSubFlag(mpv_handle *mpv, const char *name, bool value)
{
    if (mpv) {
        int flag = value ? 1 : 0;
        mpv_set_property(mpv, name, MPV_FORMAT_FLAG, &flag);
    }
}

double subDouble(mpv_handle *mpv, const char *name, double fallback = 0.0)
{
    double value = fallback;
    if (mpv) {
        mpv_get_property(mpv, name, MPV_FORMAT_DOUBLE, &value);
    }
    return value;
}

bool subFlag(mpv_handle *mpv, const char *name, bool fallback)
{
    int flag = fallback ? 1 : 0;
    if (mpv) {
        mpv_get_property(mpv, name, MPV_FORMAT_FLAG, &flag);
    }
    return flag != 0;
}

} // namespace

void MpvPlayer::applySubtitleStyle(const SubtitleStyle &style)
{
    if (!mpv_) {
        return;
    }

    const SubtitleStyle effective = style.useDefaults ? SubtitleStyle::mpvDefaults() : style;

    // Embedded ASS/SSA subtitles carry their own styling, which mpv honours by
    // default (sub-ass-override=scale) -- so without forcing the override none
    // of the settings below would visibly do anything on most .mkv files.
    // "yes" applies our style while leaving positional ASS tags (karaoke,
    // signs) intact; "force" would flatten those too.
    mpv_set_property_string(mpv_, "sub-ass-override", style.useDefaults ? "scale" : "yes");

    setSubProperty(mpv_, "sub-font", effective.fontFamily);
    setSubProperty(mpv_, "sub-font-size", effective.fontSize);
    setSubFlag(mpv_, "sub-bold", effective.bold);

    // mpv takes colors as #AARRGGBB with FF fully opaque, which is exactly
    // QColor's HexArgb spelling, so the transparency sliders need no separate
    // property -- they are the alpha channel of these colors.
    setSubProperty(mpv_, "sub-color", effective.fontColor.name(QColor::HexArgb));
    setSubProperty(mpv_, "sub-border-color", effective.outlineColor.name(QColor::HexArgb));
    setSubProperty(mpv_, "sub-border-size", effective.outlineThickness);

    setSubProperty(mpv_, "sub-shadow-color", effective.shadowColor.name(QColor::HexArgb));
    // A disabled shadow is simply one with no offset; mpv has no on/off flag.
    setSubProperty(mpv_, "sub-shadow-offset", effective.shadowEnabled ? effective.shadowSize : 0.0);

    setSubProperty(mpv_, "sub-spacing", effective.letterSpacing);
    setSubProperty(mpv_, "sub-ass-line-spacing", effective.lineSpacing);

    setSubProperty(mpv_, "sub-pos", effective.verticalPosition);
    setSubProperty(mpv_, "sub-align-x", effective.align);
}

void MpvPlayer::setSubtitleVisible(bool visible)
{
    setSubFlag(mpv_, "sub-visibility", visible);
}

bool MpvPlayer::isSubtitleVisible() const
{
    return subFlag(mpv_, "sub-visibility", true);
}

void MpvPlayer::setSubtitleDelay(double seconds)
{
    setSubProperty(mpv_, "sub-delay", seconds);
}

double MpvPlayer::subtitleDelay() const
{
    return subDouble(mpv_, "sub-delay");
}

void MpvPlayer::setSubtitleBold(bool bold)
{
    setSubFlag(mpv_, "sub-bold", bold);
}

bool MpvPlayer::isSubtitleBold() const
{
    return subFlag(mpv_, "sub-bold", false);
}

void MpvPlayer::setSubtitleFontSize(double points)
{
    setSubProperty(mpv_, "sub-font-size", std::clamp(points, 4.0, 400.0));
}

double MpvPlayer::subtitleFontSize() const
{
    return subDouble(mpv_, "sub-font-size", 38.0);
}

void MpvPlayer::setSubtitleVerticalPosition(double position)
{
    setSubProperty(mpv_, "sub-pos", std::clamp(position, 0.0, 150.0));
}

double MpvPlayer::subtitleVerticalPosition() const
{
    return subDouble(mpv_, "sub-pos", 100.0);
}

void MpvPlayer::setSubtitleAlignX(const QString &align)
{
    setSubProperty(mpv_, "sub-align-x", align);
}

void MpvPlayer::setVideoAdjust(int brightness, int contrast, int saturation, int hue)
{
    if (!mpv_) {
        return;
    }
    const auto clamp100 = [](int value) { return std::clamp(value, -100, 100); };
    setSubProperty(mpv_, "brightness", static_cast<double>(clamp100(brightness)));
    setSubProperty(mpv_, "contrast", static_cast<double>(clamp100(contrast)));
    setSubProperty(mpv_, "saturation", static_cast<double>(clamp100(saturation)));
    setSubProperty(mpv_, "hue", static_cast<double>(clamp100(hue)));
}

void MpvPlayer::setVideoRotate(int degrees)
{
    if (!mpv_) {
        return;
    }
    // mpv only accepts the four right angles here; anything else is rounded
    // down to the nearest one rather than rejected silently.
    const int normalised = ((degrees % 360) + 360) % 360;
    const int quarter = (normalised / 90) * 90;
    mpv_set_property_string(mpv_, "video-rotate", QByteArray::number(quarter).constData());
}

int MpvPlayer::videoRotate() const
{
    if (!mpv_) {
        return 0;
    }
    int64_t degrees = 0;
    mpv_get_property(mpv_, "video-rotate", MPV_FORMAT_INT64, &degrees);
    return static_cast<int>(degrees);
}

void MpvPlayer::setFlipAndMirror(bool flip, bool mirror)
{
    if (!mpv_) {
        return;
    }
    // Rebuilt from scratch each time: the two share one filter chain, so
    // toggling either has to restate both.
    QStringList filters;
    if (mirror) {
        filters.append(QStringLiteral("hflip"));
    }
    if (flip) {
        filters.append(QStringLiteral("vflip"));
    }
    mpv_set_property_string(mpv_, "vf", filters.join(QLatin1Char(',')).toUtf8().constData());
}

void MpvPlayer::setVideoGeometry(double scaleY, double zoom)
{
    if (!mpv_) {
        return;
    }
    setSubProperty(mpv_, "video-scale-y", scaleY);
    setSubProperty(mpv_, "video-zoom", zoom);
}

void MpvPlayer::setGamma(int gamma)
{
    setSubProperty(mpv_, "gamma", static_cast<double>(std::clamp(gamma, -100, 100)));
}

namespace {

/// mpv returns list and map properties as JSON when read as a string, which
/// is far less code than walking an mpv_node by hand.
QJsonDocument propertyJson(mpv_handle *mpv, const char *name)
{
    if (!mpv) {
        return {};
    }
    char *text = mpv_get_property_string(mpv, name);
    if (!text) {
        return {};
    }
    const QJsonDocument document = QJsonDocument::fromJson(QByteArray(text));
    mpv_free(text);
    return document;
}

QString propertyString(mpv_handle *mpv, const char *name)
{
    if (!mpv) {
        return {};
    }
    char *text = mpv_get_property_string(mpv, name);
    if (!text) {
        return {};
    }
    const QString value = QString::fromUtf8(text);
    mpv_free(text);
    return value;
}

} // namespace

void MpvPlayer::setAudioDelay(double seconds)
{
    setSubProperty(mpv_, "audio-delay", seconds);
}

double MpvPlayer::audioDelay() const
{
    return subDouble(mpv_, "audio-delay");
}

QVector<MpvPlayer::AudioDevice> MpvPlayer::audioDevices() const
{
    QVector<AudioDevice> devices;
    const QJsonArray array = propertyJson(mpv_, "audio-device-list").array();
    devices.reserve(array.size());
    for (const QJsonValue &value : array) {
        const QJsonObject object = value.toObject();
        devices.append({object.value(QStringLiteral("name")).toString(),
                        object.value(QStringLiteral("description")).toString()});
    }
    return devices;
}

void MpvPlayer::setAudioDevice(const QString &name)
{
    setSubProperty(mpv_, "audio-device", name);
}

QString MpvPlayer::audioDevice() const
{
    return propertyString(mpv_, "audio-device");
}

void MpvPlayer::setAudioChannels(const QString &layout)
{
    setSubProperty(mpv_, "audio-channels", layout);
}

QString MpvPlayer::audioChannels() const
{
    return propertyString(mpv_, "audio-channels");
}

void MpvPlayer::setLoopFile(bool loop)
{
    if (mpv_) {
        mpv_set_property_string(mpv_, "loop-file", loop ? "inf" : "no");
    }
}

bool MpvPlayer::isLoopingFile() const
{
    return propertyString(mpv_, "loop-file") == QLatin1StringView("inf");
}

void MpvPlayer::setLoopPlaylist(bool loop)
{
    if (mpv_) {
        mpv_set_property_string(mpv_, "loop-playlist", loop ? "inf" : "no");
    }
}

bool MpvPlayer::isLoopingPlaylist() const
{
    return propertyString(mpv_, "loop-playlist") == QLatin1StringView("inf");
}

QVector<MpvPlayer::Chapter> MpvPlayer::chapters() const
{
    QVector<Chapter> list;
    const QJsonArray array = propertyJson(mpv_, "chapter-list").array();
    list.reserve(array.size());
    for (const QJsonValue &value : array) {
        const QJsonObject object = value.toObject();
        list.append({object.value(QStringLiteral("time")).toDouble(),
                     object.value(QStringLiteral("title")).toString()});
    }
    return list;
}

int MpvPlayer::currentChapter() const
{
    if (!mpv_) {
        return -1;
    }
    int64_t index = -1;
    mpv_get_property(mpv_, "chapter", MPV_FORMAT_INT64, &index);
    return static_cast<int>(index);
}

void MpvPlayer::setChapter(int index)
{
    if (mpv_) {
        int64_t value = index;
        mpv_set_property(mpv_, "chapter", MPV_FORMAT_INT64, &value);
    }
}

QMap<QString, QString> MpvPlayer::metadata() const
{
    QMap<QString, QString> entries;
    const QJsonObject object = propertyJson(mpv_, "metadata").object();
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        entries.insert(it.key(), it.value().toVariant().toString());
    }
    return entries;
}

QString MpvPlayer::propertyText(const QString &name) const
{
    return propertyString(mpv_, name.toUtf8().constData());
}

void MpvPlayer::setVideoFrame(bool keepAspect, double panscan)
{
    if (!mpv_) {
        return;
    }
    mpv_set_property_string(mpv_, "keepaspect", keepAspect ? "yes" : "no");
    setSubProperty(mpv_, "panscan", std::clamp(panscan, 0.0, 1.0));
}

void MpvPlayer::setAspectOverride(double ratio)
{
    setSubProperty(mpv_, "video-aspect-override", ratio);
}

int MpvPlayer::videoWidth() const
{
    if (!mpv_) {
        return 0;
    }
    int64_t width = 0;
    mpv_get_property(mpv_, "width", MPV_FORMAT_INT64, &width);
    return static_cast<int>(width);
}

int MpvPlayer::videoHeight() const
{
    if (!mpv_) {
        return 0;
    }
    int64_t height = 0;
    mpv_get_property(mpv_, "height", MPV_FORMAT_INT64, &height);
    return static_cast<int>(height);
}

QSize MpvPlayer::videoDisplaySize() const
{
    if (!mpv_) {
        return {};
    }
    // dwidth/dheight rather than width/height: these are the dimensions the
    // picture is displayed at, so they follow both the aspect ratio and any
    // rotation. Verified against libmpv -- rotating a 1280x720 file by 90
    // degrees reports 720x1280 here while width/height stay 1280x720.
    int64_t width = 0;
    int64_t height = 0;
    mpv_get_property(mpv_, "dwidth", MPV_FORMAT_INT64, &width);
    mpv_get_property(mpv_, "dheight", MPV_FORMAT_INT64, &height);
    return QSize(static_cast<int>(width), static_cast<int>(height));
}

void MpvPlayer::setVideoPan(double panX, double panY)
{
    if (!mpv_) {
        return;
    }
    // Clamped to one whole frame in each direction: past that the picture is
    // pushed entirely off screen and there is nothing left to look at.
    setSubProperty(mpv_, "video-pan-x", std::clamp(panX, -1.0, 1.0));
    setSubProperty(mpv_, "video-pan-y", std::clamp(panY, -1.0, 1.0));
}

void MpvPlayer::setEqualizerGains(const QVector<int> &gainsDb)
{
    if (!mpv_) {
        return;
    }

    const QVector<double> &freqs = Equalizer::frequencies();
    const bool anyGain = std::any_of(gainsDb.cbegin(), gainsDb.cend(),
                                      [](int gain) { return gain != 0; });

    if (gainsDb.size() != freqs.size() || !anyGain) {
        // Flat: drop the chain instead of running ten unity-gain biquads over
        // every sample for no audible difference.
        mpv_set_property_string(mpv_, "af", "");
        return;
    }

    // One biquad per band. "t=o:w=1" makes the width one octave, which is
    // what puts the bands' skirts side by side at these centre frequencies.
    // Each filter applies to all channels, so this is layout-agnostic.
    QStringList filters;
    filters.reserve(freqs.size());
    for (int band = 0; band < freqs.size(); ++band) {
        filters.append(QStringLiteral("equalizer=f=%1:t=o:w=1:g=%2")
                            .arg(freqs.at(band))
                            .arg(gainsDb.at(band)));
    }

    mpv_set_property_string(mpv_, "af", filters.join(QLatin1Char(',')).toUtf8().constData());
}

void MpvPlayer::setSubtitleJustify(const QString &justify)
{
    setSubProperty(mpv_, "sub-justify", justify);
}

bool MpvPlayer::takeScreenshot(const QString &filePath)
{
    if (!mpv_ || filePath.isEmpty()) {
        return false;
    }
    // "screenshot-to-file" rather than plain "screenshot": mpv's own naming is
    // driven by screenshot-template and it never reports back where the file
    // landed, so choosing the path here is the only way to tell the user.
    //
    // "subtitles" captures what is actually on screen -- picture plus rendered
    // subtitles -- which is what someone grabbing a frame of a subtitled film
    // expects. ("video" would drop them; "window" would bake in the OSD.)
    const QByteArray utf8Path = filePath.toUtf8();
    const int status = mpvCommand(mpv_, {"screenshot-to-file", utf8Path.constData(), "subtitles"});
    return status >= 0;
}

void MpvPlayer::showOsdMessage(const QString &text, int durationMs)
{
    // Handed to the widget rather than to mpv's "show-text". mpv only draws
    // its OSD while its renderer runs, and for audio-only files the widget
    // clears the surface itself instead of calling render() -- so every
    // message was invisible for music. Emitting keeps all the call sites as
    // they are while the drawing moves somewhere that always happens.
    emit osdMessageRequested(text, durationMs);
}


QVector<MpvPlayer::TrackInfo> MpvPlayer::audioTracks() const
{
    return tracksOfType(mpv_, QLatin1StringView("audio"));
}

void MpvPlayer::setAudioTrack(int trackId)
{
    selectTrack(mpv_, "aid", trackId);
}

void MpvPlayer::setMute(bool muted)
{
    if (!mpv_) {
        return;
    }
    int flag = muted ? 1 : 0;
    mpv_set_property(mpv_, "mute", MPV_FORMAT_FLAG, &flag);
}
