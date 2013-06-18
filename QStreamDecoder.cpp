#include "stdafx.h"
#include "QStreamDecoder.h"

//------------------------------------------
QStreamDecoder::QStreamDecoder() :
	mCodecCtx(nullptr),
	mCodec(nullptr),
	mPicture(nullptr),
	mPictureRGB(nullptr),
	mRGBBuffer(nullptr),
	mConvertCtx(nullptr)
{
	/* must be called before using avcodec lib */
	ffmpeg::avcodec_init();

	/* register all the codecs */
	ffmpeg::avcodec_register_all();

	ffmpeg::av_init_packet(&mPacket);

	// Allocate decoder
	mCodec = ffmpeg::avcodec_find_decoder(ffmpeg::CODEC_ID_H264);
	if (!mCodec)
	{
		qDebug() << "Couldn't find H264 decoder!";
		return;
	}

	mCodecCtx = ffmpeg::avcodec_alloc_context();
	mPicture = ffmpeg::avcodec_alloc_frame();

	if (mCodec->capabilities & CODEC_CAP_TRUNCATED)
		mCodecCtx->flags |= CODEC_FLAG_TRUNCATED;

	// open codec
	if (ffmpeg::avcodec_open(mCodecCtx, mCodec))
	{
		qDebug() << "Could not open codec!";
		return;
	}
}
//------------------------------------------
QStreamDecoder::~QStreamDecoder()
{

}
//------------------------------------------
bool QStreamDecoder::decodeFrame(unsigned char* bytes, int size)
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
			qDebug() << "Error while decoding frame";
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
					return false;

				// Determine required buffer size and allocate buffer
				int numBytes = ffmpeg::avpicture_get_size(ffmpeg::PIX_FMT_RGB24, mCodecCtx->width, mCodecCtx->height);

				mPictureRGB->width = mCodecCtx->width;
				mPictureRGB->height = mCodecCtx->height;

				if (mRGBBuffer)
					delete[] mRGBBuffer;

				mRGBBuffer = new uint8_t[numBytes];

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

			// Convert the frame to QImage
			if (mLastFrame.width() != w ||
				mLastFrame.height() != h ||
				mLastFrame.format() != QImage::Format::Format_RGB888)
			{
				mLastFrame = QImage(w,h,QImage::Format_RGB888);
			}

			for(int y=0; y < h; y++)
			{
				memcpy(mLastFrame.scanLine(y), mPictureRGB->data[0]+y*mPictureRGB->linesize[0], w*3);
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