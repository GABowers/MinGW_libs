#include "fouriertransformdialog.h"
#include "ui_fouriertransformdialog.h"

FourierTransformDialog::FourierTransformDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::FourierTransformDialog)
{
    ui->setupUi(this);
}

FourierTransformDialog::~FourierTransformDialog()
{
    delete ui;
}