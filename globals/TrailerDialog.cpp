#include "TrailerDialog.h"
#include "ui_TrailerDialog.h"

#include "globals/Manager.h"
#include "trailerProviders/TrailerProvider.h"

TrailerDialog::TrailerDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TrailerDialog)
{
    ui->setupUi(this);

#ifdef Q_WS_MAC
    QFont font = ui->trailers->font();
    font.setPointSize(font.pointSize()-1);
    ui->trailers->setFont(font);
#endif

    ui->results->verticalHeader()->setResizeMode(QHeaderView::ResizeToContents);
    ui->trailers->verticalHeader()->setResizeMode(QHeaderView::ResizeToContents);
    ui->trailers->horizontalHeader()->setResizeMode(0, QHeaderView::ResizeToContents);
    ui->trailers->horizontalHeader()->setResizeMode(1, QHeaderView::ResizeToContents);
    ui->searchString->setType(MyLineEdit::TypeLoading);
    ui->stackedWidget->setAnimation(QEasingCurve::OutCubic);
    ui->stackedWidget->setSpeed(400);

#ifdef Q_WS_MAC
    setWindowFlags((windowFlags() & ~Qt::WindowType_Mask) | Qt::Sheet);
#else
    setWindowFlags((windowFlags() & ~Qt::WindowType_Mask) | Qt::Dialog);
#endif

    m_qnam = new QNetworkAccessManager(this);

    foreach (TrailerProvider *provider, Manager::instance()->trailerProviders()) {
        ui->comboScraper->addItem(provider->name(), Manager::instance()->trailerProviders().indexOf(provider));
        connect(provider, SIGNAL(sigSearchDone(QList<ScraperSearchResult>)), this, SLOT(showResults(QList<ScraperSearchResult>)));
        connect(provider, SIGNAL(sigLoadDone(QList<TrailerResult>)), this, SLOT(showTrailers(QList<TrailerResult>)));
    }

    connect(ui->comboScraper, SIGNAL(currentIndexChanged(int)), this, SLOT(search()));
    connect(ui->searchString, SIGNAL(returnPressed()), this, SLOT(search()));
    connect(ui->results, SIGNAL(itemClicked(QTableWidgetItem*)), this, SLOT(resultClicked(QTableWidgetItem*)));
    connect(ui->trailers, SIGNAL(itemClicked(QTableWidgetItem*)), this, SLOT(trailerClicked(QTableWidgetItem*)));
    connect(ui->buttonBackToResults, SIGNAL(clicked()), this, SLOT(backToResults()));
    connect(ui->buttonBackToTrailers, SIGNAL(clicked()), this, SLOT(backToTrailers()));
    connect(ui->buttonDownload, SIGNAL(clicked()), this, SLOT(startDownload()));
    connect(ui->buttonCancelDownload, SIGNAL(clicked()), this, SLOT(cancelDownload()));
}

TrailerDialog::~TrailerDialog()
{
    delete ui;
}

TrailerDialog* TrailerDialog::instance(QWidget *parent)
{
    static TrailerDialog *m_instance = 0;
    if (m_instance == 0) {
        m_instance = new TrailerDialog(parent);
    }
    return m_instance;
}

void TrailerDialog::clear()
{
    ui->results->clearContents();
    ui->results->setRowCount(0);
    ui->comboScraper->setEnabled(true);
    ui->searchString->setEnabled(true);
}

int TrailerDialog::exec(Movie *movie)
{
    QSize newSize;
    newSize.setHeight(qMin(400, parentWidget()->size().height()-200));
    newSize.setWidth(qMin(600, parentWidget()->size().width()-400));
    resize(newSize);

    m_downloadInProgress = false;
    m_currentMovie = movie;
    ui->stackedWidget->setCurrentIndex(0);
    ui->searchString->setText(movie->name());
    search();
    return QDialog::exec();
}

void TrailerDialog::reject()
{
    if (m_downloadInProgress)
        cancelDownload();
    QDialog::reject();
}

void TrailerDialog::search()
{
    int index = ui->comboScraper->currentIndex();
    if (index < 0 || index >= Manager::instance()->trailerProviders().size()) {
        return;
    }
    m_providerNo = ui->comboScraper->itemData(index, Qt::UserRole).toInt();
    clear();
    ui->comboScraper->setEnabled(false);
    ui->searchString->setLoading(true);
    Manager::instance()->trailerProviders().at(m_providerNo)->searchMovie(ui->searchString->text());
}

void TrailerDialog::showResults(QList<ScraperSearchResult> results)
{
    if (results.count() != 1)
        ui->stackedWidget->slideInIdx(0);

    ui->comboScraper->setEnabled(true);
    ui->searchString->setLoading(false);
    ui->searchString->setFocus();
    foreach (const ScraperSearchResult &result, results) {
        QTableWidgetItem *item = new QTableWidgetItem(QString("%1").arg(result.name));
        item->setData(Qt::UserRole, result.id);
        int row = ui->results->rowCount();
        ui->results->insertRow(row);
        ui->results->setItem(row, 0, item);
    }

    if (results.count() == 1) {
        ui->stackedWidget->setCurrentIndex(1);
        resultClicked(ui->results->item(0, 0));
    }
}

void TrailerDialog::resultClicked(QTableWidgetItem *item)
{
    m_providerId = item->data(Qt::UserRole).toString();
    ui->trailers->clearContents();
    ui->trailers->setRowCount(0);
    ui->stackedWidget->slideInIdx(1);
    Manager::instance()->trailerProviders().at(m_providerNo)->loadMovieTrailers(item->data(Qt::UserRole).toString());
}

void TrailerDialog::showTrailers(QList<TrailerResult> trailers)
{
    m_currentTrailers = trailers;
    for (int i=0, n=trailers.size() ; i<n ; ++i) {
        TrailerResult trailer = trailers.at(i);
        int row = ui->trailers->rowCount();
        ui->trailers->insertRow(row);
        QLabel *trailerPreview = new QLabel(ui->trailers);
        trailerPreview->setMargin(4);
        trailerPreview->setPixmap(QPixmap::fromImage(trailer.previewImage.scaledToWidth(100, Qt::SmoothTransformation)));
        QTableWidgetItem *item = new QTableWidgetItem(trailer.name);
        item->setData(Qt::UserRole, i);
        ui->trailers->setCellWidget(row, 0, trailerPreview);
        ui->trailers->setItem(row, 1, new QTableWidgetItem(trailer.language));
        ui->trailers->setItem(row, 2, item);
    }
}

void TrailerDialog::trailerClicked(QTableWidgetItem *item)
{
    int row = item->row();
    if (row < 0 || row >= ui->trailers->rowCount())
        return;
    int trailerNo = ui->trailers->item(row, 2)->data(Qt::UserRole).toInt();
    if (trailerNo < 0 || trailerNo >= m_currentTrailers.count())
        return;

    TrailerResult result = m_currentTrailers.at(trailerNo);
    ui->url->setText(result.trailerUrl.toString());

    ui->buttonCancelDownload->setVisible(false);
    ui->buttonDownload->setVisible(true);
    ui->progress->clear();
    ui->progressBar->setVisible(false);
    ui->progressBar->setValue(0);

    ui->stackedWidget->slideInIdx(2);

    if (!result.previewImage.isNull())
        ui->previewImage->setPixmap(QPixmap::fromImage(result.previewImage.scaled(ui->previewImage->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    else
        ui->previewImage->setPixmap(QPixmap(":/img/film_reel.png"));
}

void TrailerDialog::backToResults()
{
    ui->stackedWidget->slideInIdx(0);
}

void TrailerDialog::backToTrailers()
{
    ui->stackedWidget->slideInIdx(1);
}

void TrailerDialog::startDownload()
{
    if (m_currentMovie->files().isEmpty())
        return;

    QFileInfo fi(m_currentMovie->files().at(0));
    m_trailerFileName = QString("%1%2%3-trailer").arg(fi.canonicalPath()).arg(QDir::separator()).arg(fi.completeBaseName());
    m_output.setFileName(QString("%1.download").arg(m_trailerFileName));

    if (!m_output.open(QIODevice::WriteOnly)) {
        return;
    }

    ui->buttonDownload->setVisible(false);
    ui->buttonCancelDownload->setVisible(true);
    ui->buttonBackToTrailers->setEnabled(false);
    ui->buttonClose3->setEnabled(false);
    ui->progressBar->setVisible(true);

    m_downloadInProgress = true;
    m_downloadReply = m_qnam->get(QNetworkRequest(QUrl(ui->url->text())));
    connect(m_downloadReply, SIGNAL(finished()), this, SLOT(downloadFinished()));
    connect(m_downloadReply, SIGNAL(downloadProgress(qint64,qint64)), this, SLOT(downloadProgress(qint64,qint64)));
    connect(m_downloadReply, SIGNAL(readyRead()), SLOT(downloadReadyRead()));
    m_downloadTime.start();
}

void TrailerDialog::cancelDownload()
{
    ui->buttonDownload->setVisible(true);
    ui->buttonCancelDownload->setVisible(false);
    ui->buttonBackToTrailers->setEnabled(true);
    ui->buttonClose3->setEnabled(true);
    ui->progressBar->setVisible(false);
    ui->progressBar->setValue(0);
    ui->progress->clear();

    m_downloadReply->abort();
    m_downloadReply->deleteLater();

    m_output.close();
    m_downloadInProgress = false;
}

void TrailerDialog::downloadProgress(qint64 received, qint64 total)
{
    ui->progressBar->setRange(0, total);
    ui->progressBar->setValue(received);

    double speed = received * 1000.0 / m_downloadTime.elapsed();
    QString unit;
    if (speed < 1024) {
        unit = "bytes/sec";
    } else if (speed < 1024*1024) {
        speed /= 1024;
        unit = "kB/s";
    } else {
        speed /= 1024*1024;
        unit = "MB/s";
    }
    ui->progress->setText(QString("%1 %2").arg(speed, 3, 'f', 1).arg(unit));
}

void TrailerDialog::downloadFinished()
{
    if (m_downloadReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 302 ||
        m_downloadReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 301) {
        qDebug() << "Got redirect" << m_downloadReply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
        m_downloadReply = m_qnam->get(QNetworkRequest(m_downloadReply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl()));
        connect(m_downloadReply, SIGNAL(finished()), this, SLOT(downloadFinished()));
        connect(m_downloadReply, SIGNAL(downloadProgress(qint64,qint64)), this, SLOT(downloadProgress(qint64,qint64)));
        connect(m_downloadReply, SIGNAL(readyRead()), SLOT(downloadReadyRead()));
        return;
    }

    m_downloadInProgress = false;
    m_output.close();

    QFile file(QString("%1.download").arg(m_trailerFileName));
    if (m_downloadReply->error() == QNetworkReply::NoError) {
        ui->progress->setText(tr("Download Finished"));
        QString extension = QUrl(ui->url->text()).path();
        if (extension.lastIndexOf(".") != -1)
            extension = extension.right(extension.length()-extension.lastIndexOf(".")-1);
        else
            extension = "mov";
        QString newFileName = QString("%1.%2").arg(m_trailerFileName).arg(extension);
        QFileInfo fi(newFileName);
        if (fi.exists()) {
            QMessageBox msgBox;
            msgBox.setText(tr("The file %1 already exists.").arg(fi.fileName()));
            msgBox.setInformativeText(tr("Do you want to overwrite it?"));
            msgBox.setStandardButtons(QMessageBox::No | QMessageBox::Yes);
            msgBox.setDefaultButton(QMessageBox::Yes);
            if (msgBox.exec() == QMessageBox::Yes) {
                QFile oldFile(newFileName);
                oldFile.remove();
                file.rename(newFileName);
            }
        } else {
            file.rename(newFileName);
        }
    } else {
        ui->progress->setText(tr("Download Canceled"));
        file.remove();
    }

    ui->buttonDownload->setVisible(true);
    ui->buttonCancelDownload->setVisible(false);
    ui->progressBar->setVisible(false);
    ui->buttonBackToTrailers->setEnabled(true);
    ui->buttonClose3->setEnabled(true);

    m_downloadReply->deleteLater();
}

void TrailerDialog::downloadReadyRead()
{
    m_output.write(m_downloadReply->readAll());
}
