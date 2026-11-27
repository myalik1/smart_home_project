#ifndef SMARTHOME_GUI_WIDGETS_RULESANDSETTINGSWIDGET_H
#define SMARTHOME_GUI_WIDGETS_RULESANDSETTINGSWIDGET_H

#include "net/DtoModels.h"

#include <QPointer>
#include <QStringList>
#include <QUrl>
#include <QVector>
#include <QWidget>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QVBoxLayout;

namespace smarthome::ui {

class AddRuleDialog;

/**
 * @brief Tab for viewing/editing automation rules and gateway connection settings.
 */
class RulesAndSettingsWidget : public QWidget {
    Q_OBJECT

public:
    explicit RulesAndSettingsWidget(const QUrl &initialGatewayUrl = QUrl(QStringLiteral("http://127.0.0.1:8001")),
                                    QWidget *parent = nullptr);

    int rulesCount() const { return rules_.size(); }
    QString gatewayUrlText() const;

    QTableWidget *rulesTable() const { return rulesTable_; }
    QPushButton *addRuleButton() const { return addRuleButton_; }
    QPushButton *refreshRulesButton() const { return refreshRulesButton_; }
    QLineEdit *gatewayUrlEdit() const { return gatewayUrlEdit_; }
    QPushButton *applyGatewayUrlButton() const { return applyUrlButton_; }
    AddRuleDialog *activeAddRuleDialog() const;

    QCheckBox *rowEnabledCheckBox(int row) const;
    QPushButton *rowDeleteButton(int row) const;

    QString diagStatusText() const;
    QString diagMqttText() const;
    QString diagDbPathText() const;
    QString diagRecordsCountText() const;

public slots:
    void setAvailableDevices(const QVector<smarthome::net::DeviceStatus> &devices);
    void setRules(const QVector<smarthome::net::RuleItem> &rules);
    void onRuleAdded(const smarthome::net::RuleItem &rule);
    void onRuleToggled(const smarthome::net::RuleItem &rule);
    void onRuleDeleted(const QString &ruleId, const QVector<smarthome::net::RuleItem> &remainingRules);
    void onApiError(const QString &endpoint, int httpStatus, const QString &errorMessage);

    void setHealthStatus(const smarthome::net::HealthStatus &health);
    void setGatewayOffline();
    void setGatewayUrl(const QUrl &url);

signals:
    void refreshRulesRequested();
    void addRuleRequested(const smarthome::net::RuleItem &rule);
    void toggleRuleRequested(const QString &ruleId, bool enabled);
    void deleteRuleRequested(const QString &ruleId);
    void gatewayUrlApplyRequested(const QUrl &gatewayUrl);
    void invalidGatewayUrlEntered(const QString &errorMessage);

private slots:
    void onAddRuleClicked();
    void onApplyGatewayUrlClicked();

private:
    void setupRulesSection(QVBoxLayout *rootLayout);
    void setupSettingsSection(QVBoxLayout *rootLayout, const QUrl &initialGatewayUrl);
    void renderRulesTable();
    void confirmAndDeleteRule(const QString &ruleId);

    static QString formatSchedule(const smarthome::net::RuleItem &rule);
    static QString formatTargetDevice(const smarthome::net::RuleItem &rule);
    static QString formatCommandSummary(const smarthome::net::RuleItem &rule);

    QLabel *rulesSummaryLabel_;
    QPushButton *addRuleButton_;
    QPushButton *refreshRulesButton_;
    QTableWidget *rulesTable_;

    QLineEdit *gatewayUrlEdit_;
    QPushButton *applyUrlButton_;
    QLabel *diagStatusLabel_;
    QLabel *diagMqttLabel_;
    QLabel *diagDbPathLabel_;
    QLabel *diagRecordsLabel_;
    QLabel *diagRulesPathLabel_;
    QLabel *diagClientsLabel_;

    QVector<smarthome::net::RuleItem> rules_;
    QStringList deviceNames_;
    QPointer<AddRuleDialog> activeDialog_;
    bool updatingTable_ = false;
};

} // namespace smarthome::ui

#endif // SMARTHOME_GUI_WIDGETS_RULESANDSETTINGSWIDGET_H
