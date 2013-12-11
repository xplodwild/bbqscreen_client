#include "stdafx.h"
#include "ShrinkableQLabel.h"

//----------------------------------------------------
ShrinkableQLabel::ShrinkableQLabel(QWidget* parent /* = 0 */) : QLabel(parent),
	mHighQuality(false)
{

}
//----------------------------------------------------
void ShrinkableQLabel::setHighQuality(bool high)
{
	mHighQuality = high;
}
//---------------------------------------------------
void ShrinkableQLabel::paintEvent(QPaintEvent *aEvent)
{
	QLabel::paintEvent(aEvent);
	_displayImage();
}
//----------------------------------------------------
void ShrinkableQLabel::setPixmap(const QPixmap& aPicture)
{
	mSource = mCurrent = aPicture;
	repaint();
}
//----------------------------------------------------
void ShrinkableQLabel::_displayImage()
{
	if (mSource.isNull()) //no image was set, don't draw anything
		return;

	float cw = width(), ch = height();
	float pw = mCurrent.width(), ph = mCurrent.height();

	if (pw > cw && ph > ch && pw/cw > ph/ch || //both width and high are bigger, ratio at high is bigger or
		pw > cw && ph <= ch || //only the width is bigger or
		pw < cw && ph < ch && cw/pw < ch/ph //both width and height is smaller, ratio at width is smaller
		)
		mCurrent = mSource.scaledToWidth(cw, mHighQuality ? Qt::SmoothTransformation : Qt::TransformationMode::FastTransformation);
	else if (pw > cw && ph > ch && pw/cw <= ph/ch || //both width and high are bigger, ratio at width is bigger or
		ph > ch && pw <= cw || //only the height is bigger or
		pw < cw && ph < ch && cw/pw > ch/ph //both width and height is smaller, ratio at height is smaller
		)
		mCurrent = mSource.scaledToHeight(ch, mHighQuality ? Qt::SmoothTransformation : Qt::TransformationMode::FastTransformation);

	int x = (cw - mCurrent.width())/2, y = (ch - mCurrent.height())/2;

	QPainter paint(this);
	paint.drawPixmap(x, y, mCurrent);
}
//----------------------------------------------------
QSize ShrinkableQLabel::getRenderSize()
{
	return mCurrent.size();
}
//----------------------------------------------------