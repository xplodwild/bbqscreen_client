#include "stdafx.h"
#include "ShrinkableQLabel.h"

#include <QtGui/QPainter>
#include <QtOpenGL/QGLWidget>

//----------------------------------------------------
ShrinkableQLabel::ShrinkableQLabel(QWidget* parent /* = 0 */) : QGraphicsView(parent),
	mHighQuality(false), mLastImagePainted(true)
{
	this->setFocusPolicy(Qt::FocusPolicy::NoFocus);

	// Setup OpenGL rendering context
	QGLFormat fmt;
	fmt.setSampleBuffers(true);
	fmt.setSamples(2);
	setViewport(new QGLWidget(fmt));

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
//---------------------------------------------------
void ShrinkableQLabel::paintEvent(QPaintEvent *aEvent)
{
	if (!mLastImagePainted)
	{
		QGraphicsView::paintEvent(aEvent);
		_displayImage();
		fitInView(0, 0, mScene->width(), mScene->height(), Qt::KeepAspectRatio);
	}
}
//----------------------------------------------------
void ShrinkableQLabel::setImage(const QImage& aPicture)
{
	mSource = aPicture;
	mLastImagePainted = false;
	repaint();
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
	return mScene->sceneRect().size();
}
//----------------------------------------------------