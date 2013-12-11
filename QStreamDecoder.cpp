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

#include "stdafx.h"
#include "QStreamDecoder.h"

#include <QtMultimedia/QAudioFormat>
#include <QtMultimedia/QAudioOutput>

QMutex QStreamDecoder::mMutex;

//------------------------------------------
QStreamDecoder::QStreamDecoder(bool isAudio) :
	mCodecCtx(nullptr),
	mCodec(nullptr),
	mPicture(nullptr),
	mPictureRGB(nullptr),
	mRGBBuffer(nullptr),
	mConvertCtx(nullptr),
	mIsAudio(isAudio),
	mBuffered(false)
{

}
//------------------------------------------
QStreamDecoder::~QStreamDecoder()
{
}
//------------------------------------------
void QStreamDecoder::decodeFrame(unsigned char* bytes, int size, bool lastRendered)
{
	mInput = bytes;
	mInputSize = size;
	mLastRendered = lastRendered;
}
//------------------------------------------
void QStreamDecoder::initialize()
{
#ifndef NEW_FFMPEG_API
	/* must be called before using avcodec lib */
	ffmpeg::avcodec_init();
#endif

	/* register all the codecs */
	ffmpeg::avcodec_register_all();

	ffmpeg::av_init_packet(&mPacket);

	// Allocate decoder
	mCodec = ffmpeg::avcodec_find_decoder(mIsAudio ? ffmpeg::CODEC_ID_AAC :
		ffmpeg::CODEC_ID_H264);
	if (!mCodec)
	{
		qDebug() << "Couldn't find " << (mIsAudio ? "H264" : "AAC LATM") << " decoder!";
		return;
	}

#ifdef NEW_FFMPEG_API
	mCodecCtx = ffmpeg::avcodec_alloc_context3(NULL);
#else
	mCodecCtx = ffmpeg::avcodec_alloc_context();
#endif

	if (mIsAudio)
		mAudioFrame = (unsigned char*) ffmpeg::av_malloc(AVCODEC_MAX_AUDIO_FRAME_SIZE);
	else
		mPicture = ffmpeg::avcodec_alloc_frame();

	if (mCodec->capabilities & CODEC_CAP_TRUNCATED)
		mCodecCtx->flags |= CODEC_FLAG_TRUNCATED;

	// open codec
#ifdef NEW_FFMPEG_API
	if (ffmpeg::avcodec_open2(mCodecCtx, mCodec, NULL))
#else
	if (ffmpeg::avcodec_open(mCodecCtx, mCodec))
#endif
	{
		qDebug() << "Could not open codec!";
		return;
	}

	if (mIsAudio)
	{
		// For audio and audio only, we directly play the decoded stream as we
		// don't need to do anything else with it.
		QAudioFormat format;

		format.setSampleRate(48000);
		format.setChannelCount(2);
		format.setSampleSize(16);
		format.setCodec("audio/pcm");
		format.setByteOrder(QAudioFormat::LittleEndian);
		format.setSampleType(QAudioFormat::UnSignedInt);

		QAudioDeviceInfo info(QAudioDeviceInfo::defaultOutputDevice());
		if (!info.isFormatSupported(format))
		{
			QMessageBox::critical(0, "Audio playback error", "Raw audio format not supported by backend, cannot play audio.");
			return;
		}

		mAudioOutput = new QAudioOutput(format);
		mAudioIO = mAudioOutput->start();
	}
}
//------------------------------------------
void QStreamDecoder::process()
{
	bool result = false;

	mMutex.lock();
	if (mCodecCtx == nullptr) initialize();

	if (mIsAudio)
	{
		result = decodeAudioFrame(mInput, mInputSize);
	}
	else
	{
		result = decodeVideoFrame(mInput, mInputSize);
	}

	mMutex.unlock();

	delete[] mInput;
	mInput = NULL;

	emit decodeFinished(result, mIsAudio);
}
//------------------------------------------
bool QStreamDecoder::decodeAudioFrame(unsigned char* bytes, int size)
{
	if (size <= 0)
		return false;

	mPacket.size = size;
	mPacket.data = bytes;

	int len, out_size;
	bool hasOutput = false;
	while (mPacket.size > 0)
	{
		int out_size = AVCODEC_MAX_AUDIO_FRAME_SIZE;
		len = ffmpeg::avcodec_decode_audio3(mCodecCtx, (short*)mAudioFrame, &out_size, &mPacket);

		if (len < 0)
		{
			qDebug() << "Error while decoding audio frame";
			return false;
		}

		if (out_size > 0)
		{
			// A frame has been decoded. Play it if we have another buffer to
			// reduce audio underrun
			if (mBuffered)
			{
				mAudioIO->write((const char*)mAudioFrame, out_size);
			}
			else
			{
				mBuffered = true;
			}

			hasOutput = true;
		}
		else
		{
			qDebug() << "Could not get audio data from this frame";
		}

		
		mPacket.size -= len;
		mPacket.data += len;
	}

	return hasOutput;
}
//------------------------------------------
bool QStreamDecoder::decodeVideoFrame(unsigned char* bytes, int size)
{
	if (size <= 0)
		return false;

	mPacket.size = size;
	mPacket.data = bytes;

	int len, got_picture;
	bool hasPicture = false;
	while (mPacket.size > 0)
	{
		len = ffmpeg::avcodec_decode_video2(mCodecCtx, mPicture, &got_picture, &mPacket);

		if (len < 0)
		{
			qDebug() << "Error while decoding video frame";
			return false;
		}

		if (got_picture)
		{
			if (!mPictureRGB || mPictureRGB->width != mCodecCtx->width || mPictureRGB->height != mCodecCtx->height)
			{
				if (mPictureRGB)
				{
					qDebug() << "Realloc'ing picture RGB ; memleak!";
					//avpicture_free((ffmpeg::AVPicture*)mPictureRGB);
					mPictureRGB = nullptr;
				}

				// Allocate an AVFrame structure
				mPictureRGB = ffmpeg::avcodec_alloc_frame();
				if(!mPictureRGB)
				{
					return false;
				}

				// Determine required buffer size and allocate buffer
				int numBytes = ffmpeg::avpicture_get_size(ffmpeg::PIX_FMT_RGB24, mCodecCtx->width, mCodecCtx->height);

				mPictureRGB->width = mCodecCtx->width;
				mPictureRGB->height = mCodecCtx->height;

				if (mRGBBuffer)
					delete[] mRGBBuffer;

				mRGBBuffer = new unsigned char[numBytes];

				// Assign appropriate parts of buffer to image planes in mPictureRGB
				avpicture_fill((ffmpeg::AVPicture *)mPictureRGB, mRGBBuffer, ffmpeg::PIX_FMT_RGB24,
					mCodecCtx->width, mCodecCtx->height);
			}

			// Convert to QImage
			int w = mCodecCtx->width;
			int h = mCodecCtx->height;

			/*if (w > 1920 || h > 1920)
				qDebug() << "Unexpected size! " << w << " x " << h;*/

			mConvertCtx = ffmpeg::sws_getCachedContext(mConvertCtx, w, h, mCodecCtx->pix_fmt, w, h, ffmpeg::PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);

			if(mConvertCtx == NULL)
			{
				qDebug() << "Cannot initialize the conversion context!";
				return false;
			}

			// Convert to RGB
			ffmpeg::sws_scale(mConvertCtx, mPicture->data, mPicture->linesize, 0, mCodecCtx->height, mPictureRGB->data, mPictureRGB->linesize);

			if (mLastRendered)
			{
				// Convert the frame to QImage
				if (mLastFrame.width() != w ||
					mLastFrame.height() != h ||
					mLastFrame.format() != QImage::Format::Format_RGB888)
				{
					mLastFrame = QImage(w,h,QImage::Format_RGB888);
				}

				for (int y=0; y < h; y++)
				{
					memcpy(mLastFrame.scanLine(y), mPictureRGB->data[0]+y*mPictureRGB->linesize[0], w*3);
				}
			}
			
			hasPicture = true;
		}
		else
		{
			qDebug() << "Could not get a full picture from this frame";
		}

		
		mPacket.size -= len;
		mPacket.data += len;
	}

	return hasPicture;
}
//------------------------------------------
QImage QStreamDecoder::getLastFrame() const
{
	return mLastFrame;
}
//------------------------------------------
