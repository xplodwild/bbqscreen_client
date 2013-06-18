#ifndef _QSTREAMDECODER_H_
#define _QSTREAMDECODER_H_

#include <QIODevice>
#include <QFile>
#include <QImage>
#include <stdint.h>

#include "QTFFmpegWrapper/ffmpeg.h"

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