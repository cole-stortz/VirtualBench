#include "src/ui/panels/addcomponentdialog.h"
#include <QDialogButtonBox>

AddComponentDialog::AddComponentDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Add/Remove Components");
    setMinimumSize(560, 360);

    QVBoxLayout* layout = new QVBoxLayout(this);

    rowContainer_ = new QWidget(this);
    rowContainerLayout_ = new QVBoxLayout(rowContainer_);
    rowContainerLayout_->addStretch();

    rowScrollArea_ = new QScrollArea(this);
    rowScrollArea_->setWidgetResizable(true);
    rowScrollArea_->setWidget(rowContainer_);
    layout->addWidget(rowScrollArea_);

    addRowButton_ = new QPushButton("+ Add component", this);
    addRowButton_->setProperty("role", "outline");
    layout->addWidget(addRowButton_);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttonBox);
}
