#include "installerwindow.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QSaveFile>
#include <QSet>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QTextCursor>
#include <QTimeZone>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QDoubleSpinBox>

namespace
{
QString humanSize(quint64 bytes)
{
    static const QStringList units = {"B", "KiB", "MiB", "GiB", "TiB"};
    double size = static_cast<double>(bytes);
    int unitIndex = 0;
    while (size >= 1024.0 && unitIndex < units.size() - 1) {
        size /= 1024.0;
        ++unitIndex;
    }

    return QString::number(size, unitIndex == 0 ? 'f' : 'f', unitIndex == 0 ? 0 : 1) + " " + units.at(unitIndex);
}

QString driveLabel(const DriveInfo &drive)
{
    const QString model = drive.model.trimmed().isEmpty() ? QStringLiteral("Unknown model") : drive.model.trimmed();
    return QString("%1 (%2, %3)").arg(drive.path, model, humanSize(drive.sizeBytes));
}

void appendDeviceNode(QTreeWidgetItem *parent, const QJsonObject &object)
{
    const QString name = object.value("path").toString(object.value("name").toString());
    const QString type = object.value("type").toString();
    const QString mountPoint = object.value("mountpoint").toString("-");
    const QString fileSystem = object.value("fstype").toString("-");
    const QString detail = QString("%1 | %2 | %3 | %4")
                               .arg(type, humanSize(object.value("size").toInteger()), fileSystem, mountPoint);

    auto *item = new QTreeWidgetItem(parent);
    item->setText(0, name);
    item->setText(1, detail);

    const QJsonArray children = object.value("children").toArray();
    for (const QJsonValue &childValue : children) {
        appendDeviceNode(item, childValue.toObject());
    }
}

QJsonArray partitionsToJson(const QVector<PlannedPartition> &partitions)
{
    QJsonArray result;
    for (const PlannedPartition &partition : partitions) {
        QJsonObject object;
        object.insert("mountPoint", partition.mountPoint);
        object.insert("fileSystem", partition.fileSystem);
        object.insert("sizeGiB", partition.sizeGiB);
        object.insert("format", partition.format);
        result.append(object);
    }

    return result;
}
}

InstallerWindow::InstallerWindow(QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle("LFS Installer");
    resize(1180, 780);

    buildUi();

    refreshDrives();
    refreshSummaries();
    updateNavigationState();
}

void InstallerWindow::buildUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(18, 18, 18, 18);
    mainLayout->setSpacing(12);

    auto *title = new QLabel("Linux From Scratch Installer");
    title->setStyleSheet("font-size: 24px; font-weight: 700;");
    mainLayout->addWidget(title);

    auto *subtitle = new QLabel("Collect system details, define storage, run your repo-driven install, then choose add-ons.");
    subtitle->setWordWrap(true);
    subtitle->setStyleSheet("color: #555;");
    mainLayout->addWidget(subtitle);

    pages_ = new QStackedWidget(this);
    pages_->addWidget(buildDetailsPage());
    pages_->addWidget(buildStoragePage());
    pages_->addWidget(buildInstallPage());
    pages_->addWidget(buildFeaturesPage());
    mainLayout->addWidget(pages_, 1);

    auto *buttonRow = new QHBoxLayout();
    buttonRow->addStretch(1);

    backButton_ = new QPushButton("Back", this);
    primaryButton_ = new QPushButton("Next", this);
    primaryButton_->setMinimumWidth(160);

    buttonRow->addWidget(backButton_);
    buttonRow->addWidget(primaryButton_);
    mainLayout->addLayout(buttonRow);

    connect(backButton_, &QPushButton::clicked, this, &InstallerWindow::handleBackAction);
    connect(primaryButton_, &QPushButton::clicked, this, &InstallerWindow::handlePrimaryAction);
    connect(pages_, &QStackedWidget::currentChanged, this, &InstallerWindow::updateNavigationState);
    connect(pages_, &QStackedWidget::currentChanged, this, &InstallerWindow::refreshSummaries);
}

QWidget *InstallerWindow::buildDetailsPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);

    auto *card = new QGroupBox("Page 1: System Details", page);
    auto *form = new QFormLayout(card);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    hostnameEdit_ = new QLineEdit(card);
    hostnameEdit_->setPlaceholderText("lfs-workstation");

    usernameEdit_ = new QLineEdit(card);
    usernameEdit_->setPlaceholderText("builder");

    passwordEdit_ = new QLineEdit(card);
    passwordEdit_->setEchoMode(QLineEdit::Password);

    timeZoneCombo_ = new QComboBox(card);
    const QList<QByteArray> timeZoneIds = QTimeZone::availableTimeZoneIds();
    for (const QByteArray &timeZoneId : timeZoneIds) {
        timeZoneCombo_->addItem(QString::fromUtf8(timeZoneId));
    }
    const QString systemZone = QString::fromUtf8(QTimeZone::systemTimeZoneId());
    const int zoneIndex = timeZoneCombo_->findText(systemZone);
    if (zoneIndex >= 0) {
        timeZoneCombo_->setCurrentIndex(zoneIndex);
    }

    form->addRow("PC name", hostnameEdit_);
    form->addRow("Username", usernameEdit_);
    form->addRow("Password", passwordEdit_);
    form->addRow("Time zone", timeZoneCombo_);

    layout->addWidget(card);

    auto *note = new QLabel("These values are exported to your install repo as environment variables and also written to `install-config.json`.");
    note->setWordWrap(true);
    note->setStyleSheet("color: #555;");
    layout->addWidget(note);
    layout->addStretch(1);

    connect(hostnameEdit_, &QLineEdit::textChanged, this, &InstallerWindow::markInstallDirty);
    connect(usernameEdit_, &QLineEdit::textChanged, this, &InstallerWindow::markInstallDirty);
    connect(passwordEdit_, &QLineEdit::textChanged, this, &InstallerWindow::markInstallDirty);
    connect(timeZoneCombo_, &QComboBox::currentTextChanged, this, &InstallerWindow::markInstallDirty);

    return page;
}

QWidget *InstallerWindow::buildStoragePage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);

    auto *header = new QLabel("Page 2: Disk Layout");
    header->setStyleSheet("font-size: 18px; font-weight: 600;");
    layout->addWidget(header);

    auto *controls = new QHBoxLayout();
    controls->addWidget(new QLabel("Target drive", page));
    driveCombo_ = new QComboBox(page);
    controls->addWidget(driveCombo_, 1);

    auto *refreshButton = new QPushButton("Refresh drives", page);
    controls->addWidget(refreshButton);
    layout->addLayout(controls);

    driveDetailsLabel_ = new QLabel(page);
    driveDetailsLabel_->setWordWrap(true);
    driveDetailsLabel_->setStyleSheet("color: #555;");
    layout->addWidget(driveDetailsLabel_);

    auto *splitter = new QSplitter(Qt::Horizontal, page);

    auto *devicePanel = new QWidget(splitter);
    auto *deviceLayout = new QVBoxLayout(devicePanel);
    deviceLayout->addWidget(new QLabel("Detected disks and partitions", devicePanel));
    deviceTree_ = new QTreeWidget(devicePanel);
    deviceTree_->setColumnCount(2);
    deviceTree_->setHeaderLabels({"Device", "Details"});
    deviceTree_->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    deviceTree_->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    deviceLayout->addWidget(deviceTree_);

    auto *plannerPanel = new QWidget(splitter);
    auto *plannerLayout = new QVBoxLayout(plannerPanel);
    plannerLayout->addWidget(new QLabel("Planned partitions", plannerPanel));

    partitionTable_ = new QTableWidget(0, 4, plannerPanel);
    partitionTable_->setHorizontalHeaderLabels({"Mount point", "Filesystem", "Size (GiB)", "Format"});
    partitionTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    partitionTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    partitionTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    partitionTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    partitionTable_->verticalHeader()->setVisible(false);
    plannerLayout->addWidget(partitionTable_, 1);

    auto *partitionButtons = new QHBoxLayout();
    auto *addButton = new QPushButton("Add partition", plannerPanel);
    auto *removeButton = new QPushButton("Remove selected", plannerPanel);
    auto *autoButton = new QPushButton("Auto layout", plannerPanel);
    partitionButtons->addWidget(addButton);
    partitionButtons->addWidget(removeButton);
    partitionButtons->addWidget(autoButton);
    partitionButtons->addStretch(1);
    plannerLayout->addLayout(partitionButtons);

    splitter->addWidget(devicePanel);
    splitter->addWidget(plannerPanel);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    layout->addWidget(splitter, 1);

    auto *note = new QLabel("This is a planner UI, not a full GParted clone. Your repo script receives the selected target disk and partition plan as JSON.");
    note->setWordWrap(true);
    note->setStyleSheet("color: #555;");
    layout->addWidget(note);

    connect(refreshButton, &QPushButton::clicked, this, &InstallerWindow::refreshDrives);
    connect(driveCombo_, &QComboBox::currentIndexChanged, this, &InstallerWindow::updateDriveDetails);
    connect(driveCombo_, &QComboBox::currentIndexChanged, this, &InstallerWindow::markInstallDirty);
    connect(addButton, &QPushButton::clicked, this, [this]() {
        addPartitionRow("/", "ext4", 40.0, true);
        markInstallDirty();
    });
    connect(removeButton, &QPushButton::clicked, this, [this]() {
        const int row = partitionTable_->currentRow();
        if (row >= 0) {
            partitionTable_->removeRow(row);
            markInstallDirty();
        }
    });
    connect(autoButton, &QPushButton::clicked, this, [this]() {
        partitionTable_->setRowCount(0);
        addPartitionRow("/boot/efi", "fat32", 0.5, true);
        addPartitionRow("/", "ext4", 40.0, true);
        addPartitionRow("/home", "ext4", 20.0, true);
        addPartitionRow("swap", "swap", 4.0, true);
        markInstallDirty();
    });

    addPartitionRow("/", "ext4", 40.0, true);
    addPartitionRow("swap", "swap", 4.0, true);

    return page;
}

QWidget *InstallerWindow::buildInstallPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);

    auto *header = new QLabel("Page 3: Repo-Driven Install");
    header->setStyleSheet("font-size: 18px; font-weight: 600;");
    layout->addWidget(header);

    auto *formCard = new QGroupBox("Install source", page);
    auto *form = new QFormLayout(formCard);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    repoUrlEdit_ = new QLineEdit(formCard);
    repoUrlEdit_->setPlaceholderText("https://github.com/you/your-lfs-repo.git");

    repoBranchEdit_ = new QLineEdit(formCard);
    repoBranchEdit_->setPlaceholderText("main");
    repoBranchEdit_->setText("main");

    scriptPathEdit_ = new QLineEdit(formCard);
    scriptPathEdit_->setPlaceholderText("install.sh");
    scriptPathEdit_->setText("install.sh");

    workRootEdit_ = new QLineEdit(formCard);
    workRootEdit_->setText("/tmp/lfs-installer");

    form->addRow("Repo URL", repoUrlEdit_);
    form->addRow("Branch", repoBranchEdit_);
    form->addRow("Install script", scriptPathEdit_);
    form->addRow("Working directory", workRootEdit_);

    layout->addWidget(formCard);

    installStatusLabel_ = new QLabel("Ready to install.");
    installStatusLabel_->setStyleSheet("font-weight: 600; color: #1b5e20;");
    layout->addWidget(installStatusLabel_);

    auto *splitter = new QSplitter(Qt::Horizontal, page);

    auto *previewPanel = new QWidget(splitter);
    auto *previewLayout = new QVBoxLayout(previewPanel);
    previewLayout->addWidget(new QLabel("Generated install configuration", previewPanel));
    configPreview_ = new QPlainTextEdit(previewPanel);
    configPreview_->setReadOnly(true);
    previewLayout->addWidget(configPreview_);

    auto *logPanel = new QWidget(splitter);
    auto *logLayout = new QVBoxLayout(logPanel);
    logLayout->addWidget(new QLabel("Install log", logPanel));
    installLog_ = new QTextEdit(logPanel);
    installLog_->setReadOnly(true);
    logLayout->addWidget(installLog_);

    splitter->addWidget(previewPanel);
    splitter->addWidget(logPanel);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    layout->addWidget(splitter, 1);

    auto *note = new QLabel("On this page, `Next` is replaced by `Install`. In this GUI-only build, pressing `Install` validates the inputs, writes the generated config, and unlocks the next page without running external commands.");
    note->setWordWrap(true);
    note->setStyleSheet("color: #555;");
    layout->addWidget(note);

    connect(repoUrlEdit_, &QLineEdit::textChanged, this, &InstallerWindow::markInstallDirty);
    connect(repoBranchEdit_, &QLineEdit::textChanged, this, &InstallerWindow::markInstallDirty);
    connect(scriptPathEdit_, &QLineEdit::textChanged, this, &InstallerWindow::markInstallDirty);
    connect(workRootEdit_, &QLineEdit::textChanged, this, &InstallerWindow::markInstallDirty);

    return page;
}

QWidget *InstallerWindow::buildFeaturesPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);

    auto *header = new QLabel("Page 4: Additional Features");
    header->setStyleSheet("font-size: 18px; font-weight: 600;");
    layout->addWidget(header);

    auto *featuresBox = new QGroupBox("Checkbox selection", page);
    auto *featuresLayout = new QVBoxLayout(featuresBox);

    const QStringList featureLabels = {
        "Enable sudo for the created user",
        "Install OpenSSH",
        "Install base development tools",
        "Enable NetworkManager",
        "Install KDE Plasma",
        "Enable serial console logging"
    };

    for (const QString &label : featureLabels) {
        auto *check = new QCheckBox(label, featuresBox);
        featuresLayout->addWidget(check);
        featureChecks_.append(check);
        connect(check, &QCheckBox::checkStateChanged, this, &InstallerWindow::refreshSummaries);
    }

    layout->addWidget(featuresBox);

    auto *summaryBox = new QGroupBox("Summary / export", page);
    auto *summaryLayout = new QVBoxLayout(summaryBox);
    summaryPreview_ = new QPlainTextEdit(summaryBox);
    summaryPreview_->setReadOnly(true);
    summaryLayout->addWidget(summaryPreview_);

    auto *exportButton = new QPushButton("Export config JSON", summaryBox);
    summaryLayout->addWidget(exportButton, 0, Qt::AlignRight);
    layout->addWidget(summaryBox, 1);

    auto *note = new QLabel("You can keep this page as a simple selector now, then wire the exported feature list into your repo later.");
    note->setWordWrap(true);
    note->setStyleSheet("color: #555;");
    layout->addWidget(note);

    connect(exportButton, &QPushButton::clicked, this, &InstallerWindow::exportConfiguration);

    return page;
}

void InstallerWindow::addPartitionRow(const QString &mountPoint, const QString &fileSystem, double sizeGiB, bool format)
{
    const int row = partitionTable_->rowCount();
    partitionTable_->insertRow(row);

    auto *mountCombo = new QComboBox(partitionTable_);
    mountCombo->setEditable(true);
    mountCombo->addItems({"/boot/efi", "/boot", "/", "/home", "/var", "swap"});
    mountCombo->setCurrentText(mountPoint);
    connect(mountCombo, &QComboBox::currentTextChanged, this, &InstallerWindow::markInstallDirty);
    partitionTable_->setCellWidget(row, 0, mountCombo);

    auto *fsCombo = new QComboBox(partitionTable_);
    fsCombo->addItems({"ext4", "xfs", "btrfs", "fat32", "swap"});
    const int fsIndex = fsCombo->findText(fileSystem);
    fsCombo->setCurrentIndex(fsIndex >= 0 ? fsIndex : 0);
    connect(fsCombo, &QComboBox::currentTextChanged, this, &InstallerWindow::markInstallDirty);
    partitionTable_->setCellWidget(row, 1, fsCombo);

    auto *sizeSpin = new QDoubleSpinBox(partitionTable_);
    sizeSpin->setRange(0.1, 102400.0);
    sizeSpin->setDecimals(1);
    sizeSpin->setValue(sizeGiB);
    connect(sizeSpin, &QDoubleSpinBox::valueChanged, this, [this](double) {
        markInstallDirty();
    });
    partitionTable_->setCellWidget(row, 2, sizeSpin);

    auto *formatCheck = new QCheckBox(partitionTable_);
    formatCheck->setChecked(format);
    connect(formatCheck, &QCheckBox::checkStateChanged, this, &InstallerWindow::markInstallDirty);
    partitionTable_->setCellWidget(row, 3, formatCheck);
}

QVector<PlannedPartition> InstallerWindow::collectPartitions() const
{
    QVector<PlannedPartition> partitions;
    for (int row = 0; row < partitionTable_->rowCount(); ++row) {
        auto *mountCombo = qobject_cast<QComboBox *>(partitionTable_->cellWidget(row, 0));
        auto *fsCombo = qobject_cast<QComboBox *>(partitionTable_->cellWidget(row, 1));
        auto *sizeSpin = qobject_cast<QDoubleSpinBox *>(partitionTable_->cellWidget(row, 2));
        auto *formatCheck = qobject_cast<QCheckBox *>(partitionTable_->cellWidget(row, 3));

        if (!mountCombo || !fsCombo || !sizeSpin || !formatCheck) {
            continue;
        }

        PlannedPartition partition;
        partition.mountPoint = mountCombo->currentText().trimmed();
        partition.fileSystem = fsCombo->currentText().trimmed();
        partition.sizeGiB = sizeSpin->value();
        partition.format = formatCheck->isChecked();
        partitions.append(partition);
    }

    return partitions;
}

QStringList InstallerWindow::collectSelectedFeatures() const
{
    QStringList features;
    for (QCheckBox *check : featureChecks_) {
        if (check->isChecked()) {
            features.append(check->text());
        }
    }

    return features;
}

DriveInfo InstallerWindow::currentDrive() const
{
    const QString path = driveCombo_->currentData().toString();
    for (const DriveInfo &drive : drives_) {
        if (drive.path == path) {
            return drive;
        }
    }

    return {};
}

bool InstallerWindow::validatePageOne()
{
    if (hostnameEdit_->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Missing value", "Enter a PC name before continuing.");
        hostnameEdit_->setFocus();
        return false;
    }

    if (usernameEdit_->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Missing value", "Enter a username before continuing.");
        usernameEdit_->setFocus();
        return false;
    }

    if (passwordEdit_->text().isEmpty()) {
        QMessageBox::warning(this, "Missing value", "Enter a password before continuing.");
        passwordEdit_->setFocus();
        return false;
    }

    return true;
}

bool InstallerWindow::validatePageTwo()
{
    if (drives_.isEmpty()) {
        QMessageBox::warning(this, "No drive selected", "No target drives were detected. Refresh the drive list or ensure `lsblk` is available.");
        return false;
    }

    const DriveInfo drive = currentDrive();
    if (drive.path.isEmpty()) {
        QMessageBox::warning(this, "No drive selected", "Select a target drive before continuing.");
        return false;
    }

    const QVector<PlannedPartition> partitions = collectPartitions();
    if (partitions.isEmpty()) {
        QMessageBox::warning(this, "No partitions", "Define at least one planned partition.");
        return false;
    }

    bool hasRoot = false;
    QSet<QString> mountPoints;
    for (const PlannedPartition &partition : partitions) {
        if (partition.mountPoint.isEmpty()) {
            QMessageBox::warning(this, "Invalid partition", "Each partition entry needs a mount point or `swap`.");
            return false;
        }
        if (mountPoints.contains(partition.mountPoint)) {
            QMessageBox::warning(this, "Duplicate mount point", QString("The mount point `%1` is defined more than once.").arg(partition.mountPoint));
            return false;
        }
        mountPoints.insert(partition.mountPoint);
        if (partition.mountPoint == "/") {
            hasRoot = true;
        }
    }

    if (!hasRoot) {
        QMessageBox::warning(this, "Missing root partition", "Add a `/` partition before continuing.");
        return false;
    }

    return true;
}

bool InstallerWindow::validatePageThreeInputs()
{
    if (repoUrlEdit_->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Missing repo URL", "Enter the repository URL that contains your install logic.");
        repoUrlEdit_->setFocus();
        return false;
    }

    if (scriptPathEdit_->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Missing script path", "Enter the install script path inside the repository.");
        scriptPathEdit_->setFocus();
        return false;
    }

    if (workRootEdit_->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Missing working directory", "Enter a working directory for repo checkout and logs.");
        workRootEdit_->setFocus();
        return false;
    }

    return true;
}

void InstallerWindow::handlePrimaryAction()
{
    const int pageIndex = pages_->currentIndex();
    if (pageIndex == 0) {
        if (validatePageOne()) {
            pages_->setCurrentIndex(1);
        }
        return;
    }

    if (pageIndex == 1) {
        if (validatePageTwo()) {
            pages_->setCurrentIndex(2);
        }
        return;
    }

    if (pageIndex == 2) {
        if (installCompleted_) {
            pages_->setCurrentIndex(3);
        } else {
            startInstall();
        }
        return;
    }

    close();
}

void InstallerWindow::handleBackAction()
{
    if (installInProgress_) {
        return;
    }

    const int previousIndex = pages_->currentIndex() - 1;
    if (previousIndex >= 0) {
        pages_->setCurrentIndex(previousIndex);
    }
}

void InstallerWindow::refreshDrives()
{
    drives_.clear();
    driveCombo_->clear();
    deviceTree_->clear();

    QProcess lsblk;
    lsblk.start("lsblk", {"--json", "-b", "-o", "NAME,SIZE,TYPE,MODEL,PATH,MOUNTPOINT,FSTYPE"});
    if (!lsblk.waitForFinished(4000) || lsblk.exitStatus() != QProcess::NormalExit || lsblk.exitCode() != 0) {
        driveDetailsLabel_->setText("Unable to query drives with `lsblk`.");
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(lsblk.readAllStandardOutput());
    const QJsonArray blockDevices = document.object().value("blockdevices").toArray();
    for (const QJsonValue &deviceValue : blockDevices) {
        const QJsonObject deviceObject = deviceValue.toObject();
        appendDeviceNode(deviceTree_->invisibleRootItem(), deviceObject);

        if (deviceObject.value("type").toString() != "disk") {
            continue;
        }

        DriveInfo drive;
        drive.name = deviceObject.value("name").toString();
        drive.path = deviceObject.value("path").toString("/dev/" + drive.name);
        drive.model = deviceObject.value("model").toString();
        drive.sizeBytes = static_cast<quint64>(deviceObject.value("size").toInteger());
        drives_.append(drive);
        driveCombo_->addItem(driveLabel(drive), drive.path);
    }

    deviceTree_->expandAll();
    updateDriveDetails();
}

void InstallerWindow::updateDriveDetails()
{
    const DriveInfo drive = currentDrive();
    if (drive.path.isEmpty()) {
        driveDetailsLabel_->setText("Select a target drive. The installer will pass this drive path to your repo script.");
        return;
    }

    driveDetailsLabel_->setText(QString("Selected: %1").arg(driveLabel(drive)));
}

void InstallerWindow::startInstall()
{
    if (installInProgress_) {
        return;
    }

    if (!validatePageOne() || !validatePageTwo() || !validatePageThreeInputs()) {
        return;
    }

    const QString workRoot = workRootEdit_->text().trimmed();
    QDir directory;
    if (!directory.mkpath(workRoot)) {
        QMessageBox::critical(this, "Working directory error", QString("Unable to create `%1`.").arg(workRoot));
        return;
    }

    currentRunDirectory_ = QDir(workRoot).filePath("run-" + QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss"));
    if (!directory.mkpath(currentRunDirectory_)) {
        QMessageBox::critical(this, "Working directory error", QString("Unable to create `%1`.").arg(currentRunDirectory_));
        return;
    }

    QSaveFile configFile(QDir(currentRunDirectory_).filePath("install-config.json"));
    if (!configFile.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, "Config write failed", "Unable to write install-config.json.");
        return;
    }
    configFile.write(buildConfigJson(true));
    if (!configFile.commit()) {
        QMessageBox::critical(this, "Config write failed", "Failed to finalize install-config.json.");
        return;
    }

    installLog_->clear();
    installLog_->append(QString("Run directory: %1").arg(currentRunDirectory_));
    installCompleted_ = false;
    installCompleted_ = true;
    installLog_->append("GUI-only mode: no clone, no script execution, no disk changes were attempted.");
    installLog_->append("The generated configuration has been written to install-config.json for review.");
    setInstallStatus("GUI-only checkpoint complete. No install command was executed.", QColor("#1b5e20"));
    updateNavigationState();
}

void InstallerWindow::exportConfiguration()
{
    const QString suggested = currentRunDirectory_.isEmpty()
                                  ? QDir::home().filePath("lfs-install-config.json")
                                  : QDir(currentRunDirectory_).filePath("lfs-install-config.json");

    const QString fileName = QFileDialog::getSaveFileName(this, "Export config", suggested, "JSON files (*.json)");
    if (fileName.isEmpty()) {
        return;
    }

    QSaveFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, "Export failed", QString("Unable to open `%1` for writing.").arg(fileName));
        return;
    }

    file.write(buildConfigJson(true));
    if (!file.commit()) {
        QMessageBox::critical(this, "Export failed", QString("Unable to save `%1`.").arg(fileName));
        return;
    }
}

void InstallerWindow::markInstallDirty()
{
    if (installInProgress_) {
        refreshSummaries();
        return;
    }

    if (installCompleted_) {
        resetInstallState("Configuration changed. Re-run install to continue.");
    }

    refreshSummaries();
    updateNavigationState();
}

void InstallerWindow::refreshSummaries()
{
    if (configPreview_) {
        configPreview_->setPlainText(QString::fromUtf8(buildConfigJson(true)));
    }
    if (summaryPreview_) {
        summaryPreview_->setPlainText(QString::fromUtf8(buildConfigJson(true)));
    }
}

void InstallerWindow::updateNavigationState()
{
    const int pageIndex = pages_->currentIndex();
    backButton_->setEnabled(pageIndex > 0 && !installInProgress_);

    primaryButton_->setEnabled(!installInProgress_);

    if (pageIndex == 2) {
        primaryButton_->setText(installCompleted_ ? "Next" : "Install");
    } else if (pageIndex == 3) {
        primaryButton_->setText("Finish");
    } else {
        primaryButton_->setText("Next");
    }

    if (installInProgress_) {
        primaryButton_->setText("Installing...");
    }
}

void InstallerWindow::setInstallStatus(const QString &message, const QColor &color)
{
    installStatusLabel_->setText(message);
    installStatusLabel_->setStyleSheet(QString("font-weight: 600; color: %1;").arg(color.name()));
}

void InstallerWindow::resetInstallState(const QString &reason)
{
    installCompleted_ = false;
    if (!reason.isEmpty()) {
        setInstallStatus(reason, QColor("#ef6c00"));
    }
}

QByteArray InstallerWindow::buildConfigJson(bool pretty) const
{
    QJsonObject document;
    document.insert("hostname", hostnameEdit_->text().trimmed());
    document.insert("username", usernameEdit_->text().trimmed());
    document.insert("password", passwordEdit_->text());
    document.insert("timeZone", timeZoneCombo_->currentText());

    const DriveInfo drive = currentDrive();
    QJsonObject driveObject;
    driveObject.insert("path", drive.path);
    driveObject.insert("name", drive.name);
    driveObject.insert("model", drive.model);
    driveObject.insert("sizeBytes", static_cast<qint64>(drive.sizeBytes));
    document.insert("targetDrive", driveObject);
    document.insert("partitions", partitionsToJson(collectPartitions()));

    QJsonObject repoObject;
    repoObject.insert("url", repoUrlEdit_->text().trimmed());
    repoObject.insert("branch", repoBranchEdit_->text().trimmed());
    repoObject.insert("scriptPath", scriptPathEdit_->text().trimmed());
    repoObject.insert("workingRoot", workRootEdit_->text().trimmed());
    repoObject.insert("currentRunDirectory", currentRunDirectory_);
    document.insert("repository", repoObject);

    const QStringList features = collectSelectedFeatures();
    QJsonArray featureArray;
    for (const QString &feature : features) {
        featureArray.append(feature);
    }
    document.insert("features", featureArray);

    document.insert("installCompleted", installCompleted_);

    return QJsonDocument(document).toJson(pretty ? QJsonDocument::Indented : QJsonDocument::Compact);
}
