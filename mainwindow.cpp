/*
 *  Copyright (C) 2015, Mike Walters <mike@flomp.net>
 *
 *  This file is part of inspectrum.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <QtWidgets>
#include <QRubberBand>

#include "mainwindow.h"
#include "util.h"

MainWindow::MainWindow()
{
    setWindowTitle(tr("inspectrum"));
    scrollArea.setWidget(&spectrogram);
    scrollArea.viewport()->installEventFilter(this);
    setCentralWidget(&scrollArea);

    dock = new SpectrogramControls(tr("Controls"), this);
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::LeftDockWidgetArea, dock);

    connect(dock, SIGNAL(openFile(QString)), this, SLOT(openFile(QString)));
    connect(dock->sampleRate, SIGNAL(textChanged(QString)), this, SLOT(setSampleRate(QString)));
    connect(dock, SIGNAL(fftSizeChanged(int)), this, SLOT(setFFTSize(int)));
    connect(dock->zoomLevelSlider, SIGNAL(valueChanged(int)), this, SLOT(setZoomLevel(int)));
    connect(dock->powerMaxSlider, SIGNAL(valueChanged(int)), &spectrogram, SLOT(setPowerMax(int)));
    connect(dock->powerMinSlider, SIGNAL(valueChanged(int)), &spectrogram, SLOT(setPowerMin(int)));
}

QRubberBand *rubberBand=NULL;
QPoint mystart;

bool MainWindow::eventFilter(QObject * /*obj*/, QEvent *event)
{
    if (event->type() == QEvent::Wheel) {
        QWheelEvent *wheelEvent = (QWheelEvent*)event;
        QSlider *slider = nullptr;
        if (QApplication::keyboardModifiers() & Qt::ControlModifier) {
            slider = dock->zoomLevelSlider;
        } else if (QApplication::keyboardModifiers() & Qt::ShiftModifier) {
            slider = dock->fftSizeSlider;
        }
        if (slider != nullptr) {
            if (wheelEvent->angleDelta().y() > 0) {
                slider->setValue(slider->value() + 1);
            } else if (wheelEvent->angleDelta().y() < 0) {
                slider->setValue(slider->value() - 1);
            }
            return true;
        }
    } else if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *mouseEvent = (QMouseEvent*)event;
        if (mouseEvent->buttons() == Qt::LeftButton) {
            mystart = (mouseEvent->pos());
            if(!rubberBand)
                rubberBand = new QRubberBand(QRubberBand::Rectangle, scrollArea.viewport());
            rubberBand->setGeometry(QRect(mystart, mystart));
            rubberBand->show();
            return true;
        }
    } else if (event->type() == QEvent::MouseMove) {
        QMouseEvent *mouseEvent = (QMouseEvent*)event;
        if (mouseEvent->buttons() == Qt::LeftButton) {
            rubberBand->setGeometry(QRect(mystart, mouseEvent->pos()).normalized()); //Area Bounding
            return true;
        }
    } else if (event->type() == QEvent::MouseButtonRelease) {
        QMouseEvent *mouseEvent = (QMouseEvent*)event;
        QRect rb = rubberBand->geometry();

        off_t topSample = spectrogram.lineToSample(scrollArea.verticalScrollBar()->value() + rb.top());
        off_t bottomSample = spectrogram.lineToSample(scrollArea.verticalScrollBar()->value() + rb.bottom());

        int offset = scrollArea.horizontalScrollBar()->value();
        int width = spectrogram.width();
        float left = (float)clamp(offset + rb.left(), 0, width) / width - 0.5;
        float right = (float)clamp(offset + rb.right(), 0, width) / width - 0.5;

        if (rb.width() > 10 && rb.height() > 10) {
            selectionTime = {topSample, bottomSample};
            selectionFreq = {left, right};

            int sampleRate = dock->sampleRate->text().toInt();
            statusBar()->showMessage(QString("Freq: %1Hz to %2Hz (%3Hz) Time: %4s to %5s (%6s / %7Hz)").arg(
                                        QString::number(left * sampleRate, 'g', 4),
                                        QString::number(right * sampleRate, 'g', 4),
                                        QString::number((right - left) * sampleRate, 'g', 4),
                                        QString::number((float)topSample / sampleRate, 'g', 4),
                                        QString::number((float)bottomSample / sampleRate, 'g', 4),
                                        QString::number((float)(bottomSample - topSample) / sampleRate, 'g', 4),
                                        QString::number((float)sampleRate / (bottomSample - topSample), 'g', 4))
            );
            emit selectionChanged(selectionTime, selectionFreq);
        } else {
            rubberBand->hide();
            rubberBand->clearMask();
            statusBar()->clearMessage();
            emit selectionCleared();
        }
        return true;
    };
    return false;
}

void MainWindow::setSampleRate(QString rate)
{
    spectrogram.setSampleRate(rate.toInt());
}

void MainWindow::changeSampleRate(int rate)
{
    spectrogram.setSampleRate(rate);
    dock->sampleRate->setText(QString::number(rate));
}

void MainWindow::setFFTSize(int size)
{
    off_t sample = getCenterSample();
    spectrogram.setFFTSize(size);
    scrollArea.verticalScrollBar()->setValue(getScrollPos(sample));
}

void MainWindow::setZoomLevel(int zoom)
{
    off_t sample = getCenterSample();
    spectrogram.setZoomLevel(zoom);
    scrollArea.verticalScrollBar()->setValue(getScrollPos(sample));
}

off_t MainWindow::getCenterSample()
{
    int height = scrollArea.height();
    return (scrollArea.verticalScrollBar()->value() + height / 2) * spectrogram.getStride();
}

int MainWindow::getScrollPos(off_t sample)
{
    int height = scrollArea.height();
    return sample / spectrogram.getStride() - height / 2;
}

void MainWindow::openFile(QString fileName)
{
    QString title="%1: %2";
    this->setWindowTitle(title.arg(QApplication::applicationName(),fileName.section('/',-1,-1)));
    spectrogram.openFile(fileName);
}
