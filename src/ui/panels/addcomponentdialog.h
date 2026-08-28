#pragma once
#include <QDialog>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QPushButton>

class AddComponentDialog : public QDialog {
    Q_OBJECT

public:
    explicit AddComponentDialog(QWidget* parent = nullptr);

private:
    QScrollArea* rowScrollArea_;
    QWidget*     rowContainer_;
    QVBoxLayout* rowContainerLayout_;
    QPushButton* addRowButton_;
};
