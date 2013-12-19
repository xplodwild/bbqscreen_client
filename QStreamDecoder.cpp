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

#define AUDIO_BUFFERING 8
#define MAX_AUDIO_DATA_PENDING 50000
#define AVCODEC_MAX_AUDIO_FRAME_SIZE 192000 // it disappeared from avcodec.h

QMutex QStreamDecoder::mMutex;

static void avlog_cb(void *, int level, const char * szFmt, va_list varg) {
	/*
	if (szFmt != NULL) {
		qDebug(szFmt, varg);
	}
	*/
}

//------------------------------------------
QStreamDecoder::QStreamDecoder(bool isAudio) :
	mCodecCtx(nullptr),
	mCodec(nullptr),
	mPicture(nullptr),
	mPictureRGB(nullptr),
	mRGBBuffer(nullptr),
	mConvertCtx(nullptr),
	mIsAudio(isAudio),
	mBuffered(0)
{

}
//------------------------------------------
QStreamDecoder::~QStreamDecoder()
{
	mAudioPlaybackRunning = false;
	if (mAudioPlaybackThread.joinable())
	{
		mAudioPlaybackThread.join();
	}
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
	/* register all the codecs */
	ffmpeg::avcodec_register_all();
	ffmpeg::av_register_all();

	ffmpeg::av_init_packet(&mPacket);

	// Allocate decoder
	mCodec = ffmpeg::avcodec_find_decoder(mIsAudio ? ffmpeg::CODEC_ID_AAC :
		ffmpeg::CODEC_ID_H264);
	if (!mCodec)
	{
		qDebug() << "Couldn't find " << (mIsAudio ? "H264" : "AAC LATM") << " decoder!";
		return;
	}

	mCodecCtx = ffmpeg::avcodec_alloc_context3(mCodec);

	if (mIsAudio)
	{
		mAudioFrame = ffmpeg::av_frame_alloc();
	}
	else
	{
		mPicture = ffmpeg::av_frame_alloc();
	}

	if (mCodec->capabilities & CODEC_CAP_TRUNCATED)
		mCodecCtx->flags |= CODEC_FLAG_TRUNCATED;

	// open codec
	if (ffmpeg::avcodec_open2(mCodecCtx, mCodec, NULL))
	{
		qDebug() << "Could not open codec!";
		return;
	}

	if (mIsAudio)
	{
		// For audio and audio only, we directly play the decoded stream as we
		// don't need to do anything else with it.
		ffmpeg::av_log_set_callback(avlog_cb);

		// AVCodec however decodes audio as float, which we can't use directly
		// (at least on my hardware).
		mResampleCtx = ffmpeg::swr_alloc();
		ffmpeg::av_opt_set_int(mResampleCtx, "in_channel_layout", AV_CH_LAYOUT_STEREO, 0);
		ffmpeg::av_opt_set_int(mResampleCtx, "out_channel_layout", AV_CH_LAYOUT_STEREO, 0);
		ffmpeg::av_opt_set_int(mResampleCtx, "in_sample_rate", 48000, 0);
		ffmpeg::av_opt_set_int(mResampleCtx, "out_sample_rate", 48000, 0);
		ffmpeg::av_opt_set_sample_fmt(mResampleCtx, "in_sample_fmt", ffmpeg::AV_SAMPLE_FMT_FLTP, 0);
		ffmpeg::av_opt_set_sample_fmt(mResampleCtx, "out_sample_fmt", ffmpeg::AV_SAMPLE_FMT_S16, 0);
		if (swr_init(mResampleCtx) < 0) {
			qDebug() << "SWR_INIT ERROR";
		}

		int dst_linesize;
		int dst_nb_samples = 4096;
		ffmpeg::av_samples_alloc(&mResampleBuffer,
			&dst_linesize,
			2,
			dst_nb_samples,
			ffmpeg::AV_SAMPLE_FMT_S16,
			0);

		// Make the output audio device
		QAudioFormat format;

		format.setSampleRate(48000);
		format.setChannelCount(2);
		format.setSampleSize(16);
		format.setSampleType(QAudioFormat::SignedInt);
		format.setByteOrder(QAudioFormat::LittleEndian);
		format.setCodec("audio/pcm");

		QAudioDeviceInfo info(QAudioDeviceInfo::defaultOutputDevice());
		if (!info.isFormatSupported(format))
		{
			QMessageBox::critical(0, "Audio playback error", "Raw audio format not supported by backend, cannot play audio.");
			return;
		}

		mAudioPlaybackRunning = true;
		mAudioPlaybackThread = std::thread(&QStreamDecoder::playbackAudioThread, this);

		mAudioOutput = new QAudioOutput(format);
		mAudioIO = mAudioOutput->start();
		mAudioOutput->setVolume(1.0);
	}
	else
	{
		// Allocate an AVFrame structure
		mPictureRGB = ffmpeg::avcodec_alloc_frame();
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
void QStreamDecoder::playbackAudioThread()
{
	while (mAudioPlaybackRunning)
	{
		// Dequeue an audio frame
		if (mAudioBufferSize.size() > 0 && mBuffered >= AUDIO_BUFFERING
			&& mAudioOutput->bytesFree() > 0)
		{
			mAudioMutex.lock();

			int bufferSize = mAudioBufferSize.front();

			// If we're too slow/accumulating delay, drop audio frames
			while (mAudioBuffer.size() > MAX_AUDIO_DATA_PENDING)
			{
				bufferSize = mAudioBufferSize.front();
				mAudioBuffer.remove(0, bufferSize);
				mAudioBufferSize.pop_front();
			}

			// Write to our audio channel
			QByteArray frame = mAudioBuffer.left(bufferSize);
			mAudioIO->write(frame);
			mAudioBuffer.remove(0, bufferSize);

			mAudioMutex.unlock();

			mAudioBufferSize.pop_front();
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
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
		out_size = AVCODEC_MAX_AUDIO_FRAME_SIZE;
		len = ffmpeg::avcodec_decode_audio4(mCodecCtx, mAudioFrame, &out_size, &mPacket);

		if (len < 0)
		{
			qDebug() << "Error while decoding audio frame";
			return false;
		}

		if (out_size > 0)
		{
			// Resample from FLOAT PLANAR to S16
			int samples_output = ffmpeg::swr_convert(mResampleCtx, &mResampleBuffer, 4096, (const uint8_t**)mAudioFrame->extended_data, mAudioFrame->nb_samples);

			if (samples_output > 0)
			{
				// A frame has been decoded. Queue it to our buffer.
				mAudioMutex.lock();

				if (mBuffered < AUDIO_BUFFERING) mBuffered++;

				mAudioBuffer.append((const char*)mResampleBuffer, samples_output*4);
				mAudioBufferSize.push_back(samples_output*4);

				mAudioMutex.unlock();
				hasOutput = true;
			}
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

			if (mLastRendered)
			{
				mConvertCtx = ffmpeg::sws_getCachedContext(mConvertCtx, w, h, mCodecCtx->pix_fmt, w, h, ffmpeg::PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);

				if(mConvertCtx == NULL)
				{
					qDebug() << "Cannot initialize the conversion context!";
					return false;
				}
			
				// Convert to RGB
				ffmpeg::sws_scale(mConvertCtx, mPicture->data, mPicture->linesize, 0, mCodecCtx->height, mPictureRGB->data, mPictureRGB->linesize);

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
