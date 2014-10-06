/************************************************************************************
    Copyright (C) 2014 Daniel P. Foose - All Rights Reserved

    This file is part of Vespucci.

    Vespucci is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Vespucci is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Vespucci.  If not, see <http://www.gnu.org/licenses/>.
***************************************************************************************/

#include "loaddataset.h"
#include "ui_loaddataset.h"
#include <QFileDialog>
#include "vespuccidataset.h"
#include "vespucciworkspace.h"

///
/// \brief LoadDataset::LoadDataset
/// \param parent QWidget parent (see QDialog)
/// \param ws Current workspace
///
LoadDataset::LoadDataset(QWidget *parent, VespucciWorkspace *ws) :
    QDialog(parent),
    ui(new Ui::LoadDataset)
{
    ui->setupUi(this);
    workspace = ws;
    directory_ = ws->directory_ptr();
    QLineEdit *name_box = this->findChild<QLineEdit *>("nameBox");
    QString name = "Dataset" + QString::number(workspace->dataset_loading_count());
    name_box->setText(name); //puts in default name

    QDialogButtonBox *button_box = this->findChild<QDialogButtonBox *>("buttonBox");
    QPushButton *ok_button = button_box->button(QDialogButtonBox::Ok);
    ok_button->setDisabled(true);
}

LoadDataset::~LoadDataset()
{
    delete ui;
}

///
/// \brief LoadDataset::on_browseButton_clicked
/// Triggers dialog to open input file
void LoadDataset::on_browseButton_clicked()
{
    QString filename;
    QLineEdit *filename_line_edit = this->findChild<QLineEdit *>("filenameBox");

    filename = QFileDialog::getOpenFileName(this, tr("Open Data File"),
                                            workspace->directory(),
                                            tr("Text Files (*.txt);;"
                                               "SPC Files (*.spc);;"
                                               "Vespucci Dataset Files (*.vds);;"));
    filename_line_edit->setText(filename);

}

///
/// \brief LoadDataset::on_buttonBox_accepted
/// Loads dataset from the file into a new dataset object by triggering appropriate
/// constructor.
void LoadDataset::on_buttonBox_accepted()
{

    QCheckBox *swap_check_box = this->findChild<QCheckBox *>("swapCheckBox");
    QLineEdit *filename_line_edit = this->findChild<QLineEdit *>("filenameBox");
    QLineEdit *name_box = this->findChild<QLineEdit *>("nameBox");
    QLineEdit *y_description_box = this->findChild<QLineEdit *>("yDescription");
    QLineEdit *x_description_box = this->findChild<QLineEdit *>("xDescription");
    QComboBox *y_units_box = this->findChild<QComboBox *>("yUnits");
    QComboBox *x_units_box = this->findChild<QComboBox *>("xUnits");

    QString y_description = y_description_box->text() + " (" + y_units_box->currentText() + ")";
    QString x_description = x_description_box->text() + " (" + x_units_box->currentText() + ")";

    QString name = name_box->text();
    QString filename = filename_line_edit->text();
    QFileInfo file_info(filename);

    //bool remove_at_position = false;
    //int temp_position=0;
    //int position;
    bool swap = swap_check_box->isChecked();

    QStringList names_list = workspace->dataset_names();
    QStringList::iterator i;

    int warning_response=QMessageBox::Yes;
    //Warning message pops up when user enters a name that already exists
    for (i=names_list.begin(); i != names_list.end(); ++i){
        if (name==(*i)){
            warning_response =
                    QMessageBox::question(this,
                                          "Duplicate Name",
                                          "There is already a dataset with this"
                                          " name in the workspace. Datasets are"
                                          " not indexed by name, but using two"
                                          " datasets with the same name may lead"
                                          " to confusion. Are you sure you wish"
                                          " to continue with this name?",
                                          QMessageBox::No,
                                          QMessageBox::Yes);
        }
    }

    if (warning_response == QMessageBox::Yes && file_info.exists()){
        if (file_info.suffix() == "txt"){
            QFile *log_file = workspace->CreateLogFile(name);
            QSharedPointer<VespucciDataset> data(new VespucciDataset(filename,
                                                     workspace->main_window(),
                                                     directory_,
                                                     log_file,
                                                     name,
                                                     x_description,
                                                     y_description,
                                                     swap));
            if (!data->ConstructorCancelled()){
                workspace->AddDataset(data);
                workspace->set_directory(file_info.dir().absolutePath());
            }
        }

        if (file_info.suffix() == "vds"){
            QMessageBox::critical(this, "Feature not Implemented", "This file type is not supported yet.");
            return;
            //QSharedPointer<VespucciDataset> data(new VespucciDataset(filename,
            //                                         workspace->main_window(),
            //                                         directory_));
            //if (!data->ConstructorCancelled()){
            //    data->SetName(name);
            //    workspace->AddDataset(data);
            //    workspace->set_directory(file_info.dir().absolutePath());
            //}
        }


    }
    this->close();
}


///
/// \brief LoadDataset::on_filenameBox_textChanged
/// \param arg1
/// Changes file info displays when file name is changed
void LoadDataset::on_filenameBox_textChanged(const QString &arg1)
{
    QLabel *file_size_label = this->findChild<QLabel *>("fileSize");
    QFileInfo file_info(arg1);
    QDialogButtonBox *button_box = this->findChild<QDialogButtonBox *>("buttonBox");
    QPushButton *ok_button = button_box->button(QDialogButtonBox::Ok);

    if(file_info.exists()){
        double file_size = file_info.size()/1048576;
        QString file_size_string = QString::number(file_size, 'f', 3);
        file_size_label->setText(file_size_string);
        ok_button->setEnabled(true);
    }

    else{
        file_size_label->setText("File does not exist!");
        ok_button->setDisabled(true);
    }

}

///
/// \brief LoadDataset::on_buttonBox_rejected
/// Closes window when "Cancel" is selected.
void LoadDataset::on_buttonBox_rejected()
{
    this->close();
}
