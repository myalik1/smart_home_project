#ifndef SMARTHOME_GUI_WIDGETS_ADDRULEDIALOG_H
#define SMARTHOME_GUI_WIDGETS_ADDRULEDIALOG_H

#include "net/DtoModels.h"

#include <QDialog>
#include <QStringList>

class QCheckBox;
class QComboBox;
class QDateTimeEdit;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QTimeEdit;

namespace smarthome::ui {

/**
 * @brief Modal dialog for creating an automation rule (POST /api/v1/rules/items).
 */
class AddRuleDialog : public QDialog {
    Q_OBJECT

public:
    explicit AddRuleDialog(const QStringList &deviceNames, QWidget *parent = nullptr);

    smarthome::net::RuleItem buildRuleItem() const;

    QLineEdit *idEdit() const { return idEdit_; }
    QComboBox *typeCombo() const { return typeCombo_; }
    QCheckBox *enabledCheckBox() const { return enabledCheck_; }
    QTimeEdit *pointTimeEdit() const { return pointTimeEdit_; }
    QCheckBox *repeatDailyCheckBox() const { return repeatDailyCheck_; }
    QTimeEdit *startTimeEdit() const { return startTimeEdit_; }
    QTimeEdit *endTimeEdit() const { return endTimeEdit_; }
    QSpinBox *intervalSecondsSpinBox() const { return intervalSecondsSpin_; }
    QSpinBox *maxTicksSpinBox() const { return maxTicksSpin_; }
    QDateTimeEdit *timestampDateTimeEdit() const { return timestampEdit_; }
    QComboBox *deviceCombo() const { return deviceCombo_; }
    QComboBox *commandCombo() const { return commandCombo_; }
    QDoubleSpinBox *valueSpinBox() const { return valueSpin_; }
    QComboBox *modeCombo() const { return modeCombo_; }
    QPushButton *createButton() const { return createButton_; }
    QLabel *errorLabel() const { return errorLabel_; }

public slots:
    void handleSubmitSuccess();
    void handleSubmitError(int httpStatus, const QString &message);

signals:
    void ruleSubmitted(const smarthome::net::RuleItem &rule);

private:
    void onTypeChanged(const QString &type);
    void onCommandChanged(const QString &command);
    void onSubmitClicked();
    void showInlineError(const QString &message);

    QJsonObject buildActionCommandJson() const;
    QJsonObject buildEndActionCommandJson() const;

    QLineEdit *idEdit_;
    QComboBox *typeCombo_;
    QCheckBox *enabledCheck_;

    QStackedWidget *scheduleStack_;
    QTimeEdit *pointTimeEdit_;
    QCheckBox *repeatDailyCheck_;
    QTimeEdit *startTimeEdit_;
    QTimeEdit *endTimeEdit_;
    QCheckBox *windowRepeatDailyCheck_;
    QSpinBox *intervalSecondsSpin_;
    QSpinBox *maxTicksSpin_;
    QDateTimeEdit *timestampEdit_;

    QComboBox *deviceCombo_;
    QComboBox *commandCombo_;
    QLabel *valueLabel_;
    QDoubleSpinBox *valueSpin_;
    QLabel *modeLabel_;
    QComboBox *modeCombo_;

    QLabel *errorLabel_;
    QPushButton *createButton_;
    QPushButton *cancelButton_;
};

} // namespace smarthome::ui

#endif // SMARTHOME_GUI_WIDGETS_ADDRULEDIALOG_H
