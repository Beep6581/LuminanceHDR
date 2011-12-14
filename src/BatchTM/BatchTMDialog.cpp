/**
 * This file is a part of LuminanceHDR package.
 * ----------------------------------------------------------------------
 * Copyright (C) 2007 Giuseppe Rota
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * ----------------------------------------------------------------------
 *
 * Original Work
 * @author Giuseppe Rota <grota@users.sourceforge.net>
 * Improvements, bugfixing 
 * @author Franco Comida <fcomida@users.sourceforge.net>
 * Improvements, bugfixing
 * @author Davide Anastasia <davideanastasia@users.sourceforge.net>
 *
 */

#include <climits>
#include <cassert>

#ifdef QT_DEBUG
#include <QDebug>
#endif

#include <QFileDialog>
#include <QTextStream>

#include "BatchTM/BatchTMDialog.h"
#include "ui_BatchTMDialog.h"

#include "Common/config.h"
#include "Exif/ExifOperations.h"
#include "Core/TonemappingOptions.h"
#include "BatchTM/BatchTMJob.h"

BatchTMDialog::BatchTMDialog(QWidget *p):
    QDialog(p), m_Ui(new Ui::BatchTMDialog),
    start_left(-1), stop_left(-1), start_right(-1), stop_right(-1)
{
    //qRegisterMetaType<QImage>("QImage");    // What's its meaning?!
#ifdef QT_DEBUG
    qDebug() << "BatchTMDialog::BatchTMDialog()";
#endif
    m_Ui->setupUi(this);

    setWindowModality(Qt::WindowModal);   // Mac Mode

    m_batchTmInputDir = m_luminance_options.getBatchTmPathHdrInput();
    m_batchTmTmoSettingsDir = m_luminance_options.getBatchTmPathTmoSettings();
    m_batchTmOutputDir = m_luminance_options.getBatchTmPathLdrOutput();
    m_max_num_threads = m_luminance_options.getBatchTmNumThreads();

    connect(m_Ui->add_dir_HDRs_Button,    SIGNAL(clicked()), this, SLOT(add_dir_HDRs())       );
    connect(m_Ui->add_HDRs_Button,        SIGNAL(clicked()), this, SLOT(add_HDRs())           );
    connect(m_Ui->add_dir_TMopts_Button,  SIGNAL(clicked()), this, SLOT(add_dir_TMopts())     );
    connect(m_Ui->add_TMopts_Button,      SIGNAL(clicked()), this, SLOT(add_TMopts())         );
    connect(m_Ui->out_folder_Button,      SIGNAL(clicked()), this, SLOT(out_folder_clicked()) );
    connect(m_Ui->remove_HDRs_Button,     SIGNAL(clicked()), this, SLOT(remove_HDRs())        );
    connect(m_Ui->remove_TMOpts_Button,   SIGNAL(clicked()), this, SLOT(remove_TMOpts())      );
    connect(m_Ui->BatchGoButton,          SIGNAL(clicked()), this, SLOT(batch_core()));  //start_called()

    connect(m_Ui->filterLineEdit,         SIGNAL(textChanged(const QString&)), this, SLOT(filterChanged(const QString&)));
    connect(m_Ui->filterComboBox,         SIGNAL(activated(int)), this, SLOT(filterComboBoxActivated(int)));

    full_Log_Model  = new QStringListModel();
    log_filter      = new QSortFilterProxyModel(this);
    log_filter->setDynamicSortFilter(true);
    log_filter->setSourceModel(full_Log_Model);
    m_Ui->Log_Widget->setModel(log_filter);
    m_Ui->Log_Widget->setWordWrap(true);

    m_thread_slot.release(m_max_num_threads);
    m_available_threads = new bool[m_max_num_threads];
    for (int r = 0; r < m_max_num_threads; r++) m_available_threads[r] = TRUE;  // reset to true

    m_next_hdr_file = 0;
    m_is_batch_running  = false;

    add_log_message(tr("Using %1 thread(s)").arg(m_max_num_threads));
    add_log_message(tr("Saving using file format: %1").arg(m_luminance_options.getBatchTmLdrFormat()));
}

BatchTMDialog::~BatchTMDialog()
{
    //printf("BatchTMDialog::~BatchTMDialog()\n");
    this->hide();

    delete log_filter;
    delete full_Log_Model;
    delete [] m_available_threads;

    QApplication::restoreOverrideCursor();
}

void BatchTMDialog::add_dir_HDRs()
{
    //printf("BatchTMDialog::add_dir_HDRs()\n");

    QString dirname=QFileDialog::getExistingDirectory(this, tr("Choose a directory"), m_batchTmInputDir );
    if ( !dirname.isEmpty() )
    {
        QStringList filters;
        filters << "*.exr" << "*.hdr" << "*.pic" << "*.tiff" << "*.tif" << "*.pfs" << "*.crw" << "*.cr2" << "*.nef" << "*.dng" << "*.mrw" << "*.orf" << "*.kdc" << "*.dcr" << "*.arw" << "*.raf" << "*.ptx" << "*.pef" << "*.x3f" << "*.raw" << "*.sr2" << "*.rw2" << "*.srw";
        filters << "*.EXR" << "*.HDR" << "*.PIC" << "*.TIFF" << "*.TIF" << "*.PFS" << "*.CRW" << "*.CR2" << "*.NEF" << "*.DNG" << "*.MRW" << "*.ORF" << "*.KDC" << "*.DCR" << "*.ARW" << "*.RAF" << "*.PTX" << "*.PEF" << "*.X3F" << "*.RAW" << "*.SR2" << "*.RW2" << "*.SRW";
        QDir chosendir(dirname);
        chosendir.setFilter(QDir::Files);
        chosendir.setNameFilters(filters);
        QStringList onlyhdrs=chosendir.entryList();
        //hack to prepend to this list the path as prefix.
        onlyhdrs.replaceInStrings(QRegExp("(.+)"), chosendir.path()+"/\\1");
        add_view_model_HDRs(onlyhdrs);
    }
}

void BatchTMDialog::add_HDRs()
{
    //printf("BatchTMDialog::add_HDRs()\n");

    QString filetypes = tr("All HDR images ");
    filetypes += "(*.exr *.hdr *.pic *.tiff *.tif *.pfs *.crw *.cr2 *.nef *.dng *.mrw *.orf *.kdc *.dcr *.arw *.raf *.ptx *.pef *.x3f *.raw *.sr2 *.rw2 *.srw "
                 "*.EXR *.HDR *.PIC *.TIFF *.TIF *.PFS *.CRW *.CR2 *.NEF *.DNG *.MRW *.ORF *.KDC *.DCR *.ARW *.RAF *.PTX *.PEF *.X3F *.RAW *.SR2 *.RW2 *.SRW)";
    QStringList onlyhdrs = QFileDialog::getOpenFileNames(this, tr("Select input images"), m_batchTmInputDir, filetypes);
    add_view_model_HDRs(onlyhdrs);
}

void BatchTMDialog::add_dir_TMopts()
{
    //printf("BatchTMDialog::add_dir_TMopts()\n");

    QString dirname = QFileDialog::getExistingDirectory(this, tr("Choose a directory"), m_batchTmTmoSettingsDir);
    if ( !dirname.isEmpty() )
    {
        QStringList filters;
        filters << "*.txt";
        QDir chosendir(dirname);
        chosendir.setFilter(QDir::Files);
        chosendir.setNameFilters(filters);
        QStringList onlytxts = chosendir.entryList();
        //hack to prepend to this list the path as prefix.
        onlytxts.replaceInStrings(QRegExp("(.+)"), chosendir.path()+"/\\1");
        add_view_model_TM_OPTs(onlytxts);
    }
}

void BatchTMDialog::add_TMopts()
{
    //printf("BatchTMDialog::add_TMopts()\n");

    QStringList onlytxts = QFileDialog::getOpenFileNames(this,
                                                         tr("Load tone mapping settings text files..."),
                                                         m_batchTmTmoSettingsDir,
                                                         tr("LuminanceHDR tone mapping settings text file (*.txt)"));
    add_view_model_TM_OPTs(onlytxts);
}

TonemappingOptions* BatchTMDialog::parse_tm_opt_file(QString fname)
{
    //printf("BatchTMDialog::parse_tm_opt_file()\n");

    try
    {
        return TMOptionsOperations::parseFile(fname);
    }
    catch (QString &e)
    {
        add_log_message(e);
        return NULL;
    }
}

void BatchTMDialog::check_enable_start()
{
    //printf("BatchTMDialog::check_enable_start()\n");

    //at least 1 hdr AND at least 1 tm_opt AND output_dir not empty
    m_Ui->BatchGoButton->setEnabled(
            (!m_Ui->out_folder_widgets->text().isEmpty()) &&
            (m_Ui->listWidget_HDRs->count() != 0) &&
            (m_Ui->listWidget_TMopts->count() != 0)
            );
}

void BatchTMDialog::out_folder_clicked()
{
    //printf("BatchTMDialog::out_folder_clicked()\n");

    QString dirname = QFileDialog::getExistingDirectory(this, tr("Choose a directory"), m_batchTmOutputDir);

    QFileInfo test(dirname);
    if (test.isWritable() && test.exists() && test.isDir() && !dirname.isEmpty())
    {
        m_batchTmOutputDir = dirname;
        m_luminance_options.setBatchTmPathLdrOutput(m_batchTmOutputDir);

        m_Ui->out_folder_widgets->setText(dirname);
        check_enable_start();
    }
}

void BatchTMDialog::add_view_model_HDRs(QStringList list)
{
    //printf("BatchTMDialog::add_view_model_HDRs()\n");

    for (int idx = 0; idx < list.size(); ++idx)
    {
        //fill graphical list
        m_Ui->listWidget_HDRs->addItem(QFileInfo(list.at(idx)).fileName());
    }
    HDRs_list += list;
    check_enable_start();
}

void BatchTMDialog::add_view_model_TM_OPTs(QStringList list)
{
    //printf("BatchTMDialog::add_view_model_TM_OPTs()\n");

    for (int idx = 0; idx < list.size(); ++idx)
    {
        QString curr_tmo_options_file = list.at(idx);

        TonemappingOptions *i_th_tm_opt = parse_tm_opt_file(curr_tmo_options_file);
        if (i_th_tm_opt != NULL)
        {
            //add to data structure
            m_tm_options_list.append(i_th_tm_opt);

            //add to UI
            m_Ui->listWidget_TMopts->addItem(QFileInfo(curr_tmo_options_file).fileName());
        }
    }
    check_enable_start();
}

//TODO: questa merda deve essere sistemata... stasera!
void BatchTMDialog::update_selection_interval(bool left)
{
    //printf("BatchTMDialog::update_selection_interval()\n");

    if (left)
    {
        start_left = m_Ui->listWidget_HDRs->count();
        stop_left = -1;
        for ( int i = 0; i < m_Ui->listWidget_HDRs->count(); ++i )
        {
            if ( m_Ui->listWidget_HDRs->isItemSelected(m_Ui->listWidget_HDRs->item(i)) )
            {
                start_left= (start_left>i) ? i : start_left;
                stop_left= (stop_left<i) ? i : stop_left;
            }
        }
    }
    else
    {
        start_right = m_Ui->listWidget_TMopts->count();
        stop_right = -1;
        for ( int i = 0; i < m_Ui->listWidget_TMopts->count(); ++i )
        {
            if ( m_Ui->listWidget_TMopts->isItemSelected(m_Ui->listWidget_TMopts->item(i)) )
            {
                start_right= (start_right>i) ? i : start_right;
                stop_right= (stop_right<i) ? i : stop_right;
            }
        }
    }
}

void BatchTMDialog::remove_HDRs()
{
    //printf("BatchTMDialog::remove_HDRs()\n");

    update_selection_interval(true);
    if ( m_Ui->listWidget_HDRs->count()==0 || start_left==-1 || stop_left==-1 )
    {
        return;
    }

    for ( int i = (stop_left - start_left + 1); i > 0; i--)
    {
        m_Ui->listWidget_HDRs->takeItem(start_left);
        HDRs_list.removeAt(start_left);
    }
    start_left = stop_left = -1;
    check_enable_start();
}

void BatchTMDialog::remove_TMOpts()
{
    //printf("BatchTMDialog::remove_TMOpts()\n");

    update_selection_interval(false);
    if ( (m_Ui->listWidget_TMopts->count() == 0) || (start_right == -1) || (stop_right == -1) )
    {
        return;
    }

    for (int i = (stop_right - start_right + 1); i > 0; i--)
    {
        m_Ui->listWidget_TMopts->takeItem(start_right);
        m_tm_options_list.removeAt(start_right);
    }
    start_right = stop_right = -1;
    check_enable_start();
}

void BatchTMDialog::add_log_message(const QString& message)
{ 
    // Mutex lock!
    m_add_log_message_mutex.lock();

    //qDebug() << qPrintable(message);
    full_Log_Model->insertRows(full_Log_Model->rowCount(), 1);
    full_Log_Model->setData(full_Log_Model->index(full_Log_Model->rowCount()-1), message, Qt::DisplayRole);
    m_Ui->Log_Widget->scrollToBottom();

    // Mutex unlock!
    m_add_log_message_mutex.unlock();
}

void BatchTMDialog::filterChanged(const QString& newtext)
{
    //printf("BatchTMDialog::filterChanged()\n");

    bool no_text = newtext.isEmpty();
    m_Ui->filterComboBox->setEnabled(no_text);
    m_Ui->filterLabel1->setEnabled(no_text);
    m_Ui->clearTextToolButton->setEnabled(!no_text);
    if (no_text)
    {
        filterComboBoxActivated(m_Ui->filterComboBox->currentIndex());
    }
    else
    {
        log_filter->setFilterRegExp(QRegExp(newtext, Qt::CaseInsensitive, QRegExp::RegExp));
    }
}

void BatchTMDialog::filterComboBoxActivated(int index)
{
    //printf("BatchTMDialog::filterComboBoxActivated()\n");

    QString regexp;
    switch (index)
    {
    case 0:
        regexp = ".*";
        break;
    case 1:
        regexp = "error";
        break;
    case 2:
        regexp = "successful";
        break;
    }
    log_filter->setFilterRegExp(QRegExp(regexp, Qt::CaseInsensitive, QRegExp::RegExp));
}



// Davide Anastasia <davideanastasia@users.sourceforge.net>

void BatchTMDialog::batch_core()
{
    //printf("BatchTMDialog::batch_core()\n");
    init_batch_tm_ui();

    start_batch_thread(); // kick off the conversion!
}


void BatchTMDialog::start_batch_thread()
{
    // lock the class resources in order to work free of race-condition
    m_class_data_mutex.lock();

    if ( m_next_hdr_file == HDRs_list.size() )
    {
        m_class_data_mutex.unlock();
        emit stop_batch_tm_ui();
    }
    else
    {
        int t_id = get_available_thread_id();
        if ( t_id != INT_MAX )
        {
            // at least one thread free!
            // start thread
            // I create the thread with NEW, but I let it die on its own, so don't need to store its pointer somewhere
            BatchTMJob * job_thread = new BatchTMJob(t_id, HDRs_list.at(m_next_hdr_file), &m_tm_options_list, m_Ui->out_folder_widgets->text());

            // Thread deletes itself when it has done with its job
            connect(job_thread, SIGNAL(finished()),
                    job_thread, SLOT(deleteLater()));
            connect(job_thread, SIGNAL(done(int)),
                    this, SLOT(release_thread(int))); //, Qt::DirectConnection);
            connect(job_thread, SIGNAL(add_log_message(const QString &)),
                    this, SLOT(add_log_message(const QString &))); //, Qt::DirectConnection);
            connect(job_thread, SIGNAL(increment_progress_bar(int)),
                    this, SLOT(increment_progress_bar(int))); //, Qt::DirectConnection);

            job_thread->start();
            m_next_hdr_file++;

            m_class_data_mutex.unlock();
            emit start_batch_thread();
        }
        else
        {
            m_class_data_mutex.unlock();
            // no thread available!
            // I return without doing anything!
            return;
        }
    }
}

int BatchTMDialog::get_available_thread_id()
{  
    if ( m_thread_slot.tryAcquire() )
    {
        // if I'm here, at least one thread is available

        m_thread_control_mutex.lock();
        int t_id;
        // look for the first available ID
        for (t_id = 0; t_id < m_max_num_threads; t_id++)
        {
            if (m_available_threads[t_id] == TRUE)
            {
                m_available_threads[t_id] = FALSE;
                break;
            }
        }
        m_thread_control_mutex.unlock();

        //printf("BatchTMDialog::get_available_thread_id(): t_id = %d, max_num_threads = %d\n", t_id, m_max_num_threads);
        assert ( t_id != m_max_num_threads );
        return (t_id);
    }
    else return INT_MAX;
}

void BatchTMDialog::release_thread(int t_id)
{
    //printf("BatchTMDialog::release_thread()\n");

    m_thread_control_mutex.lock();
    m_available_threads[t_id] = true;
    m_thread_control_mutex.unlock();

    m_thread_slot.release();

    emit start_batch_thread();
}

void BatchTMDialog::init_batch_tm_ui()
{
    //printf("BatchTMDialog::init_batch_tm_ui()\n");

    m_is_batch_running = true;

    // progress bar activated
    m_Ui->overallProgressBar->setMaximum( m_Ui->listWidget_HDRs->count()*(m_Ui->listWidget_TMopts->count() + 1) );
    m_Ui->overallProgressBar->setValue(0);

    // disable all buttons!
    m_Ui->cancelbutton->setDisabled(true);
    m_Ui->BatchGoButton->setDisabled(true);
    m_Ui->out_folder_Button->setDisabled(true);
    m_Ui->add_dir_HDRs_Button->setDisabled(true);
    m_Ui->add_HDRs_Button->setDisabled(true);
    m_Ui->remove_HDRs_Button->setDisabled(true);
    m_Ui->add_dir_TMopts_Button->setDisabled(true);
    m_Ui->add_TMopts_Button->setDisabled(true);
    m_Ui->remove_TMOpts_Button->setDisabled(true);

    // mouse pointer to busy
    QApplication::setOverrideCursor(QCursor(Qt::BusyCursor));

    // change the label of the processing button
    m_Ui->BatchGoButton->setText(tr("Processing..."));

    add_log_message(tr("Start processing..."));
}

void BatchTMDialog::stop_batch_tm_ui()
{
    if ( m_thread_slot.tryAcquire(m_max_num_threads) )
    {
        m_Ui->cancelbutton->setDisabled(false);
        m_Ui->cancelbutton->setText(tr("Close"));

        m_Ui->BatchGoButton->setText(tr("&Done"));
        add_log_message(tr("All tasks completed."));
        QApplication::restoreOverrideCursor();

        m_is_batch_running = false;

        m_thread_slot.release(m_max_num_threads);
    }
}

void BatchTMDialog::closeEvent( QCloseEvent* ce )
{
#ifdef QT_DEBUG
    qDebug() << "BatchTMDialog::closeEvent()";
#endif
    if ( m_is_batch_running )
        ce->ignore();
    else
        ce->accept();
}


void BatchTMDialog::increment_progress_bar(int inc)
{
    m_Ui->overallProgressBar->setValue(m_Ui->overallProgressBar->value()+inc);
}