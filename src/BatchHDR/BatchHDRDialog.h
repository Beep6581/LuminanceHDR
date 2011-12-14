/**
 * This file is a part of Luminance HDR package.
 * ----------------------------------------------------------------------
 * Copyright (C) 2011 Franco Comida
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
 * @author Franco Comida <fcomida@users.sourceforge.net>
 *
 */

#ifndef BATCH_HDR_IMPL_H
#define BATCH_HDR_IMPL_H

#include <QDialog>


#include "Common/LuminanceOptions.h"

// Forward declaration
class IOWorker;
class HdrCreationManager;

namespace Ui {
    class BatchHDRDialog;
}

class BatchHDRDialog: public QDialog
{
Q_OBJECT
private:
    Ui::BatchHDRDialog* m_Ui;
public:
	BatchHDRDialog(QWidget *parent = 0);
	~BatchHDRDialog();

protected slots:
	void num_bracketed_changed(int);
	void add_input_directory();
	void add_output_directory(QString dir = QString());
	void init_batch_hdr();
	void batch_hdr();
	void align(QStringList);
	void create_hdr();
	void error_while_loading(QString);
	void writeAisData(QByteArray);
	void check_start_button();

protected:
        LuminanceOptions m_luminance_options;

        //Application-wide settings, loaded via QSettings
        QString m_batchHdrInputDir;
        QString m_batchHdrOutputDir;
        QString m_tempDir;

	QStringList m_bracketed;
	IOWorker *m_IO_Worker;
	HdrCreationManager *m_hdrCreationManager;
	int m_numProcessed;
	bool m_errors;
};

#endif