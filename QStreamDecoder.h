/**
 * Copyright (C) 2013 Guillaume Lesniak
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef _QSTREAMDECODER_H_
#define _QSTREAMDECODER_H_

#include <QIODevice>
#include <QFile>
#include <QImage>

#ifdef NEW_FFMPEG_API
namespace ffmpeg {
extern "C" {
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}
}
#else
#include "QTFFmpegWrapper/ffmpeg.h"
#endif

class QStreamDecoder : public QObject
{
    Q_OBJECT;

public:
    // ctor
    QStreamDecoder();

    // dtor
    ~QStreamDecoder();

    // Decode a frame
    bool decodeFrame(unsigned char* bytes, int size);

    // Returns the last decoded frame as QImage
    QImage getLastFrame() const;

protected:
    ffmpeg::AVCodec* mCodec;
    ffmpeg::AVCodecContext* mCodecCtx;
    ffmpeg::AVPacket mPacket;
    ffmpeg::AVFrame* mPicture;
    ffmpeg::AVFrame* mPictureRGB;
    uint8_t* mRGBBuffer;

    QImage mLastFrame;
    ffmpeg::SwsContext* mConvertCtx;
};



#endif
