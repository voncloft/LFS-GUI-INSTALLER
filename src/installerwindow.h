#pragma once

#include <QColor>
#include <QVector>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QStackedWidget;
class QTableWidget;
class QTextEdit;
class QTreeWidget;

struct PlannedPartition
{
    QString mountPoint;
    QString fileSystem;
    double sizeGiB = 0.0;
    bool format = true;
};

struct DriveInfo
{
    QString path;
    QString name;
    QString model;
    quint64 sizeBytes = 0;
};

class InstallerWindow : public QWidget
{
    Q_OBJECT

public:
    explicit InstallerWindow(QWidget *parent = nullptr);

private slots:
    void handlePrimaryAction();
    void handleBackAction();
    void refreshDrives();
    void updateDriveDetails();
    void startInstall();
    void exportConfiguration();
    void markInstallDirty();
    void refreshSummaries();
    void refreshPartitionEditorPreview();

private:
    void buildUi();
    QWidget *buildDetailsPage();
    QWidget *buildStoragePage();
    QWidget *buildInstallPage();
    QWidget *buildFeaturesPage();
    void addPartitionRow(const QString &mountPoint, const QString &fileSystem, double sizeGiB, bool format);
    QVector<PlannedPartition> collectPartitions() const;
    QStringList collectSelectedFeatures() const;
    DriveInfo currentDrive() const;
    bool validatePageOne();
    bool validatePageTwo();
    bool validatePageThreeInputs();
    void updateNavigationState();
    void setInstallStatus(const QString &message, const QColor &color);
    void resetInstallState(const QString &reason);
    QByteArray buildConfigJson(bool pretty) const;

    QStackedWidget *pages_ = nullptr;
    QPushButton *backButton_ = nullptr;
    QPushButton *primaryButton_ = nullptr;

    QLineEdit *hostnameEdit_ = nullptr;
    QLineEdit *usernameEdit_ = nullptr;
    QLineEdit *passwordEdit_ = nullptr;
    QComboBox *timeZoneCombo_ = nullptr;

    QComboBox *driveCombo_ = nullptr;
    QLabel *driveDetailsLabel_ = nullptr;
    QTreeWidget *deviceTree_ = nullptr;
    QComboBox *newPartitionMountCombo_ = nullptr;
    QComboBox *newPartitionFsCombo_ = nullptr;
    QDoubleSpinBox *newPartitionSizeSpin_ = nullptr;
    QCheckBox *newPartitionFormatCheck_ = nullptr;
    QLabel *newPartitionRemainingLabel_ = nullptr;
    QPushButton *newPartitionAddButton_ = nullptr;
    QTableWidget *partitionTable_ = nullptr;
    QWidget *partitionMapWidget_ = nullptr;
    QLabel *partitionCapacityLabel_ = nullptr;
    QLabel *partitionOperationsLabel_ = nullptr;

    QLineEdit *repoUrlEdit_ = nullptr;
    QLineEdit *repoBranchEdit_ = nullptr;
    QLineEdit *scriptPathEdit_ = nullptr;
    QLineEdit *workRootEdit_ = nullptr;
    QLabel *installStatusLabel_ = nullptr;
    QProgressBar *installProgressBar_ = nullptr;
    QPlainTextEdit *configPreview_ = nullptr;
    QTextEdit *installLog_ = nullptr;
    QPushButton *pageThreeBackButton_ = nullptr;
    QPushButton *pageThreeInstallButton_ = nullptr;

    QVector<QCheckBox *> featureChecks_;
    QPlainTextEdit *summaryPreview_ = nullptr;

    QVector<DriveInfo> drives_;
    bool installCompleted_ = false;
    bool installInProgress_ = false;
    QString currentRunDirectory_;
};
