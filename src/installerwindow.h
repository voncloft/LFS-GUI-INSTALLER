#pragma once

#include <QProcess>
#include <QColor>
#include <QVector>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QNetworkAccessManager;
class QNetworkReply;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QStackedWidget;
class QTableWidget;
class QTreeWidget;

struct PlannedPartition
{
    QString mountPoint;
    QString localMountPoint;
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
    void handleInstallProcessOutput();
    void handleInstallProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void handleInstallProcessError(QProcess::ProcessError error);
    void handleFeatureRepoReply(QNetworkReply *reply);

private:
    void buildUi();
    QWidget *buildDetailsPage();
    QWidget *buildStoragePage();
    QWidget *buildInstallPage();
    QWidget *buildFeaturesPage();
    void addPartitionRow(const QString &mountPoint,
                         const QString &localMountPoint,
                         const QString &fileSystem,
                         double sizeGiB,
                         bool format);
    QVector<PlannedPartition> collectPartitions() const;
    QStringList collectSelectedFeatures() const;
    DriveInfo currentDrive() const;
    bool validatePageOne();
    bool validatePageTwo();
    bool validatePageThreeInputs();
    void updateNavigationState();
    void setInstallStatus(const QString &message, const QColor &color);
    void resetInstallState(const QString &reason);
    QString findScriptsDirectory() const;
    bool generateInstallArtifacts(const QString &sourceScriptsDirectory,
                                  QString *runtimeScriptsDirectory,
                                  QString *errorMessage) const;
    QStringList collectScriptPaths(const QString &scriptsDirectory) const;
    int countScriptSteps(const QStringList &scriptPaths) const;
    void populateFeaturePackages();
    void applyFeatureFilters();
    void loadFeaturePackagesFromRepo();
    void queueFeatureMetadataRequests();
    void requestFeatureMetadataForItem(QListWidgetItem *item);
    void requestCurrentFeatureMetadata();
    void startNextInstallScript();
    void appendInstallLogLine(const QString &line);
    void processInstallOutputLine(const QString &line);
    QString buildFeatureOutputText() const;
    QString buildFinalSetupScript() const;
    QString buildPartitionScript() const;
    QString buildMountScript() const;
    QString buildHostnameFile() const;
    QString buildClockFile() const;
    QString buildFstabFile() const;
    QString buildConfigText() const;
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
    QComboBox *newPartitionLocalMountCombo_ = nullptr;
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
    QPlainTextEdit *installLog_ = nullptr;
    QPushButton *pageThreeBackButton_ = nullptr;
    QPushButton *pageThreeInstallButton_ = nullptr;

    QLineEdit *featureSearchEdit_ = nullptr;
    QListWidget *featureListWidget_ = nullptr;
    QLabel *featureCountLabel_ = nullptr;
    QPlainTextEdit *featureOutput_ = nullptr;
    QPushButton *featureOutdatedButton_ = nullptr;
    QNetworkAccessManager *featureRepoManager_ = nullptr;
    int featureRepoRequestGeneration_ = 0;
    int activeFeatureMetadataRequests_ = 0;

    QVector<DriveInfo> drives_;
    QProcess *installProcess_ = nullptr;
    QStringList installScriptPaths_;
    QString installOutputBuffer_;
    QString pendingInstallStepText_;
    QString currentInstallScriptPath_;
    QString currentRuntimeScriptsDirectory_;
    int currentInstallScriptIndex_ = -1;
    int totalInstallSteps_ = 0;
    int completedInstallSteps_ = 0;
    bool installCompleted_ = false;
    bool installInProgress_ = false;
    bool refreshingSummaries_ = false;
    QString currentRunDirectory_;
};
