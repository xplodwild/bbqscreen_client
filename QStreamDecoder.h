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
#include <QtMultimedia/QAudioOutput>
#include <QIODevice>
#include <QThread>
#include <QMutex>

#include <thread>
#include <mutex>

#include <QTFFmpegWrapper/ffmpeg.h>


class QStreamDecoder : public QObject
{
	Q_OBJECT;

public:
	// ctor
	QStreamDecoder(bool isAudio);

	// dtor
	~QStreamDecoder();

	// Returns the last decoded frame as QImage
	QImage getLastFrame() const;

public slots:
	void decodeFrame(unsigned char* bytes, int size, bool lastRendered = true);
	void process();

signals:
	void decodeFinished(bool result, bool isAudio);

protected:
	void initialize();

	void playbackAudioThread();

	bool decodeVideoFrame(unsigned char* bytes, int size);
	bool decodeAudioFrame(unsigned char* bytes, int size);

protected:
	static QMutex mMutex;
	std::thread mAudioPlaybackThread;
	std::mutex mAudioMutex;
	bool mAudioPlaybackRunning;

	unsigned char* mInput;
	int mInputSize;
	bool mLastRendered;

	bool mIsAudio;
	ffmpeg::AVCodec* mCodec;
	ffmpeg::AVCodecContext* mCodecCtx;
	ffmpeg::SwrContext* mResampleCtx;
	ffmpeg::AVPacket mPacket;
	ffmpeg::AVFrame* mPicture;
	ffmpeg::AVFrame* mPictureRGB;
	ffmpeg::AVFrame* mAudioFrame;
	unsigned char* mRGBBuffer;
	uint8_t* mResampleBuffer;

	QAudioOutput* mAudioOutput;
	QIODevice* mAudioIO;
	QByteArray mAudioBuffer;
	QList<int> mAudioBufferSize;
	int mBuffered;

	QImage mLastFrame;
	ffmpeg::SwsContext* mConvertCtx;
};



#endif
