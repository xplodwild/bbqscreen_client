#include "stdafx.h"
#include "ShrinkableQLabel.h"
#include "mainwindow.h"

#include <QtGui/QPainter>
#include <QtOpenGL/QGLWidget>

//----------------------------------------------------
ShrinkableQLabel::ShrinkableQLabel(QWidget* parent /* = 0 */) : QGraphicsView(parent),
	mHighQuality(false)
{
	this->setFocusPolicy(Qt::FocusPolicy::NoFocus);
	this->setFrameStyle(QFrame::NoFrame);
	this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	// Setup OpenGL rendering context
	QGLFormat fmt;
	fmt.setSampleBuffers(false);
	fmt.setDoubleBuffer(true);
	fmt.setDirectRendering(true);
	fmt.setSwapInterval(1);
	fmt.setStencil(false);
	fmt.setRgba(false);
	fmt.setDepth(false);
	setViewport(new QGLWidget(fmt));
	viewport()->setAttribute(Qt::WA_OpaquePaintEvent);
	viewport()->setAttribute(Qt::WA_NoSystemBackground);

	// Setup our scene (which is just the pixmap)
	mScene = new QGraphicsScene(this);
	setScene(mScene);
	mPixmapItem = new QGraphicsPixmapItem(0);
	mScene->addItem(mPixmapItem);
}
//----------------------------------------------------
void ShrinkableQLabel::setHighQuality(bool high)
{
	mHighQuality = high;
}
//----------------------------------------------------
void ShrinkableQLabel::setImage(const QImage& aPicture)
{
	mSource = aPicture;
	_displayImage();
	fitInView(0, 0, mScene->width(), mScene->height(), Qt::KeepAspectRatio);
}
//----------------------------------------------------
void ShrinkableQLabel::_displayImage()
{
	QPixmap pixmap = QPixmap::fromImage(mSource);
	mPixmapItem->setTransformationMode(mHighQuality ? Qt::SmoothTransformation : Qt::TransformationMode::FastTransformation);
	mPixmapItem->setPixmap(pixmap);
	mScene->setSceneRect(mPixmapItem->boundingRect());
}
//----------------------------------------------------
QSizeF ShrinkableQLabel::getRenderSize()
{
	QSizeF s = QSizeF(mScene->width(), mScene->height());
	float ratio = 1.0f;
	if (mScene->height() > mScene->width()) {
		ratio = (mScene->height() / height());
	} else {
		ratio = (mScene->width() / width());
	}
	
	if (ratio == 0) ratio = 1.0f;

	return s / ratio;
}
//----------------------------------------------------
