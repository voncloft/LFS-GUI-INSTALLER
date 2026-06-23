#include "installerwindow.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QFontDatabase>
#include <QFrame>
#include <QGroupBox>
#include <QGridLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProcessEnvironment>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QStatusBar>
#include <QStandardPaths>
#include <QToolBar>
#include <QToolButton>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QSet>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTextCursor>
#include <QTimeZone>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QDirIterator>
#include <QListWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QDoubleSpinBox>

#include <unistd.h>

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
        object.insert("localMountPoint", partition.localMountPoint);
        object.insert("fileSystem", partition.fileSystem);
        object.insert("sizeGiB", partition.sizeGiB);
        object.insert("format", partition.format);
        result.append(object);
    }

    return result;
}

double bytesToGiB(quint64 bytes)
{
    return static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
}

QString partitionNodeName(const QString &drivePath, int row)
{
    const QString basePath = drivePath.isEmpty() ? QStringLiteral("/dev/sdX") : drivePath;
    const QString separator = !basePath.isEmpty() && basePath.back().isDigit() ? QStringLiteral("p") : QString();
    return QString("%1%2%3").arg(basePath, separator).arg(row + 1);
}

QString partitionLabelText(const QString &mountPoint)
{
    if (mountPoint == "/boot/efi") {
        return "EFI";
    }
    if (mountPoint == "/boot") {
        return "BOOT";
    }
    if (mountPoint == "/") {
        return "ROOT";
    }
    if (mountPoint == "/home") {
        return "HOME";
    }
    if (mountPoint == "/var") {
        return "VAR";
    }
    if (mountPoint == "swap") {
        return "SWAP";
    }

    QString label = mountPoint;
    label.remove('/');
    return label.isEmpty() ? QStringLiteral("CUSTOM") : label.toUpper();
}

QString defaultLocalMountPoint(const QString &mountPoint)
{
    const QString trimmed = mountPoint.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }
    if (trimmed == "/") {
        return QStringLiteral("/mnt/lfs");
    }
    if (trimmed.startsWith('/')) {
        return QStringLiteral("/mnt/lfs") + trimmed;
    }
    if (trimmed == "swap") {
        return QStringLiteral("/mnt/lfs/swap");
    }

    return QStringLiteral("/mnt/lfs/") + trimmed;
}

QStringList localMountPointOptions()
{
    return {
        QString(),
        QStringLiteral("/mnt/lfs"),
        QStringLiteral("/mnt/lfs/boot"),
        QStringLiteral("/mnt/lfs/boot/efi"),
        QStringLiteral("/mnt/lfs/home"),
        QStringLiteral("/mnt/lfs/var"),
        QStringLiteral("/mnt/lfs/swap")
    };
}

constexpr int FeatureKeyRole = Qt::UserRole + 1;
constexpr int FeatureVersionRole = Qt::UserRole + 2;
constexpr int FeatureDescriptionRole = Qt::UserRole + 3;
constexpr int FeatureRawUrlRole = Qt::UserRole + 4;
constexpr int FeatureMetadataLoadedRole = Qt::UserRole + 5;
constexpr int FeatureMetadataLoadingRole = Qt::UserRole + 6;
constexpr int FeatureRepoPathRole = Qt::UserRole + 7;

QString featureDisplayText(const QString &category,
                           const QString &packageName,
                           const QString &version,
                           const QString &description)
{
    QString text = QString("[%1] %2").arg(category, packageName);
    if (!version.trimmed().isEmpty()) {
        text += QString(" - %1").arg(version.trimmed());
    }
    if (!description.trimmed().isEmpty()) {
        text += QString(": %1").arg(description.trimmed());
    }

    return text;
}

QString githubReposTreeUrl()
{
    return QStringLiteral("https://api.github.com/repos/voncloft/Voncloft-OS/git/trees/master?recursive=1");
}

QString githubRawPrefix()
{
    return QStringLiteral("https://raw.githubusercontent.com/voncloft/Voncloft-OS/master/");
}

constexpr int MaxConcurrentFeatureMetadataRequests = 6;

QColor fileSystemColor(const QString &fileSystem)
{
    const QString normalized = fileSystem.toLower();
    if (normalized == "fat32") {
        return QColor("#344f74");
    }
    if (normalized == "ext4") {
        return QColor("#f3ef9a");
    }
    if (normalized == "xfs") {
        return QColor("#b7d8f6");
    }
    if (normalized == "btrfs") {
        return QColor("#e58a43");
    }
    if (normalized == "swap") {
        return QColor("#a6a6a6");
    }

    return QColor("#d1d8e0");
}

QString flagsText(const PlannedPartition &partition)
{
    QStringList flags;
    if (partition.mountPoint == "/boot/efi") {
        flags << "boot" << "esp";
    }
    if (partition.mountPoint == "swap") {
        flags << "swap";
    }
    flags << (partition.format ? "format" : "keep");
    return flags.join(", ");
}

QString usedText(const PlannedPartition &partition)
{
    return partition.format ? QStringLiteral("0.00 GiB") : QStringLiteral("---");
}

QString unusedText(const PlannedPartition &partition)
{
    if (!partition.format) {
        return QStringLiteral("---");
    }

    return QString("%1 GiB").arg(QString::number(partition.sizeGiB, 'f', 2));
}

QString shellQuote(const QString &value)
{
    QString escaped = value;
    escaped.replace('\'', "'\"'\"'");
    return QString("'%1'").arg(escaped);
}

QString partedFileSystem(const PlannedPartition &partition)
{
    if (partition.mountPoint == "/boot/efi") {
        return QStringLiteral("fat32");
    }

    const QString normalized = partition.fileSystem.trimmed().toLower();
    if (partition.mountPoint == "swap" || normalized == "swap") {
        return QStringLiteral("linux-swap");
    }

    return normalized.isEmpty() ? QStringLiteral("ext4") : normalized;
}

QString fstabFileSystem(const PlannedPartition &partition)
{
    const QString normalized = partition.fileSystem.trimmed().toLower();
    if (partition.mountPoint == "swap" || normalized == "swap") {
        return QStringLiteral("swap");
    }
    if (normalized == "fat32") {
        return QStringLiteral("vfat");
    }

    return normalized.isEmpty() ? QStringLiteral("auto") : normalized;
}

QString mkfsCommand(const PlannedPartition &partition, const QString &devicePath)
{
    if (!partition.format) {
        return QString("# format disabled for %1").arg(devicePath);
    }

    const QString normalized = partition.fileSystem.trimmed().toLower();
    const QString quotedDevice = shellQuote(devicePath);
    if (partition.mountPoint == "swap" || normalized == "swap") {
        return QString("mkswap %1").arg(quotedDevice);
    }
    if (normalized == "fat32") {
        return QString("mkfs.fat -F 32 %1").arg(quotedDevice);
    }
    if (normalized == "ext4") {
        return QString("mkfs.ext4 -F %1").arg(quotedDevice);
    }
    if (normalized == "xfs") {
        return QString("mkfs.xfs -f %1").arg(quotedDevice);
    }
    if (normalized == "btrfs") {
        return QString("mkfs.btrfs -f %1").arg(quotedDevice);
    }

    return QString("# unsupported filesystem %1 for %2").arg(normalized, devicePath);
}

int fstabPassNumber(const PlannedPartition &partition)
{
    if (partition.mountPoint == "swap") {
        return 0;
    }

    return partition.mountPoint == "/" ? 1 : 2;
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
    buttonRow->setContentsMargins(12, 0, 12, 0);
    backButton_ = new QPushButton("Back", this);
    primaryButton_ = new QPushButton("Next", this);
    backButton_->setFixedSize(150, 50);
    primaryButton_->setFixedSize(150, 50);

    buttonRow->addWidget(backButton_);
    buttonRow->addStretch(1);
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
    layout->setContentsMargins(12, 12, 12, 12);

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
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(6);

    auto *menuBar = new QMenuBar(page);
    auto *storageMenu = menuBar->addMenu("Storage");
    auto *editMenu = menuBar->addMenu("Edit");
    auto *viewMenu = menuBar->addMenu("View");
    auto *deviceMenu = menuBar->addMenu("Device");
    auto *partitionMenu = menuBar->addMenu("Partition");
    auto *helpMenu = menuBar->addMenu("Help");
    layout->addWidget(menuBar);

    auto *toolbar = new QToolBar(page);
    toolbar->setMovable(false);
    toolbar->setFloatable(false);
    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    auto *refreshAction = toolbar->addAction("Refresh Devices");
    auto *addAction = toolbar->addAction("New");
    auto *removeAction = toolbar->addAction("Delete");
    auto *resizeAction = toolbar->addAction("Resize/Move");
    auto *copyAction = toolbar->addAction("Copy");
    auto *pasteAction = toolbar->addAction("Paste");
    toolbar->addSeparator();
    toolbar->addWidget(new QLabel("Device:", toolbar));
    driveCombo_ = new QComboBox(toolbar);
    driveCombo_->setMinimumContentsLength(28);
    driveCombo_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    toolbar->addWidget(driveCombo_);
    layout->addWidget(toolbar);

    auto *refreshMenuAction = storageMenu->addAction("Refresh Devices");
    editMenu->addAction("Undo Last Operation")->setEnabled(false);
    editMenu->addAction("Clear All Operations")->setEnabled(false);
    auto *pendingOpsAction = viewMenu->addAction("Pending Operations");
    pendingOpsAction->setCheckable(true);
    pendingOpsAction->setChecked(true);
    deviceMenu->addAction("Create Partition Table")->setEnabled(false);
    auto *newMenuAction = partitionMenu->addAction("New");
    auto *deleteMenuAction = partitionMenu->addAction("Delete");
    partitionMenu->addSeparator();
    partitionMenu->addAction("Resize/Move")->setEnabled(false);
    partitionMenu->addAction("Copy")->setEnabled(false);
    partitionMenu->addAction("Paste")->setEnabled(false);
    helpMenu->addAction("About Partition Editor")->setEnabled(false);

    resizeAction->setEnabled(false);
    copyAction->setEnabled(false);
    pasteAction->setEnabled(false);

    auto *plannerBox = new QGroupBox("Partitions", page);
    auto *plannerLayout = new QVBoxLayout(plannerBox);
    plannerLayout->setSpacing(10);

    driveDetailsLabel_ = new QLabel(plannerBox);
    driveDetailsLabel_->setWordWrap(true);
    plannerLayout->addWidget(driveDetailsLabel_);

    auto *mapBox = new QGroupBox("Visual Disk Layout", plannerBox);
    auto *mapLayout = new QVBoxLayout(mapBox);

    auto *mapHeaderLayout = new QHBoxLayout();
    mapHeaderLayout->addWidget(new QLabel("Visual layout", mapBox));
    mapHeaderLayout->addStretch(1);
    partitionCapacityLabel_ = new QLabel(mapBox);
    mapHeaderLayout->addWidget(partitionCapacityLabel_);
    mapLayout->addLayout(mapHeaderLayout);

    partitionMapWidget_ = new QWidget(mapBox);
    auto *partitionMapLayout = new QHBoxLayout(partitionMapWidget_);
    partitionMapLayout->setContentsMargins(0, 0, 0, 0);
    partitionMapLayout->setSpacing(6);
    mapLayout->addWidget(partitionMapWidget_);

    plannerLayout->addWidget(mapBox);

    auto *tableBox = new QGroupBox("Partition List", plannerBox);
    auto *tableLayout = new QVBoxLayout(tableBox);

    auto *createBox = new QGroupBox("New Partition", tableBox);
    auto *createLayout = new QHBoxLayout(createBox);
    createLayout->addWidget(new QLabel("Mount point", createBox));

    newPartitionMountCombo_ = new QComboBox(createBox);
    newPartitionMountCombo_->setEditable(true);
    newPartitionMountCombo_->addItems({"", "/", "/boot", "/boot/efi", "/home", "/var", "swap"});
    newPartitionMountCombo_->setMinimumContentsLength(12);
    newPartitionMountCombo_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    createLayout->addWidget(newPartitionMountCombo_, 1);

    createLayout->addWidget(new QLabel("Local mount", createBox));
    newPartitionLocalMountCombo_ = new QComboBox(createBox);
    newPartitionLocalMountCombo_->setEditable(true);
    newPartitionLocalMountCombo_->addItems(localMountPointOptions());
    newPartitionLocalMountCombo_->setCurrentText("/mnt/lfs");
    newPartitionLocalMountCombo_->setMinimumContentsLength(14);
    newPartitionLocalMountCombo_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    createLayout->addWidget(newPartitionLocalMountCombo_, 2);

    createLayout->addWidget(new QLabel("Filesystem", createBox));
    newPartitionFsCombo_ = new QComboBox(createBox);
    newPartitionFsCombo_->addItems({"ext4", "xfs", "btrfs", "fat32", "swap"});
    createLayout->addWidget(newPartitionFsCombo_);

    createLayout->addWidget(new QLabel("Size", createBox));
    newPartitionSizeSpin_ = new QDoubleSpinBox(createBox);
    newPartitionSizeSpin_->setRange(0.1, 102400.0);
    newPartitionSizeSpin_->setDecimals(1);
    newPartitionSizeSpin_->setSuffix(" GiB");
    newPartitionSizeSpin_->setValue(1.0);
    createLayout->addWidget(newPartitionSizeSpin_);

    newPartitionFormatCheck_ = new QCheckBox("Format", createBox);
    newPartitionFormatCheck_->setChecked(true);
    createLayout->addWidget(newPartitionFormatCheck_);

    newPartitionRemainingLabel_ = new QLabel("Remaining: select a drive", createBox);
    createLayout->addWidget(newPartitionRemainingLabel_);

    newPartitionAddButton_ = new QPushButton("Add Partition", createBox);
    createLayout->addWidget(newPartitionAddButton_);
    createLayout->addStretch(1);

    tableLayout->addWidget(createBox);

    partitionTable_ = new QTableWidget(0, 9, tableBox);
    partitionTable_->setHorizontalHeaderLabels({"Partition", "File System", "Mount Point", "Local Mount", "Label", "Size", "Used", "Unused", "Flags"});
    partitionTable_->setAlternatingRowColors(true);
    partitionTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    partitionTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    partitionTable_->setShowGrid(true);
    partitionTable_->verticalHeader()->setVisible(false);
    partitionTable_->horizontalHeader()->setStretchLastSection(false);
    partitionTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    partitionTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    partitionTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    partitionTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    partitionTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    partitionTable_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    partitionTable_->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    partitionTable_->horizontalHeader()->setSectionResizeMode(7, QHeaderView::ResizeToContents);
    partitionTable_->horizontalHeader()->setSectionResizeMode(8, QHeaderView::ResizeToContents);
    partitionTable_->verticalHeader()->setDefaultSectionSize(34);
    tableLayout->addWidget(partitionTable_);
    plannerLayout->addWidget(tableBox, 1);

    auto *plannerStatus = new QStatusBar(plannerBox);
    partitionOperationsLabel_ = new QLabel("0 operations pending", plannerStatus);
    plannerStatus->addWidget(partitionOperationsLabel_);
    plannerLayout->addWidget(plannerStatus);
    layout->addWidget(plannerBox, 1);

    connect(refreshAction, &QAction::triggered, this, &InstallerWindow::refreshDrives);
    connect(refreshMenuAction, &QAction::triggered, this, &InstallerWindow::refreshDrives);
    connect(driveCombo_, &QComboBox::currentIndexChanged, this, &InstallerWindow::updateDriveDetails);
    connect(driveCombo_, &QComboBox::currentIndexChanged, this, &InstallerWindow::markInstallDirty);
    connect(newPartitionMountCombo_, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        const QString mountPoint = text.trimmed();
        if (newPartitionLocalMountCombo_) {
            newPartitionLocalMountCombo_->setCurrentText(defaultLocalMountPoint(mountPoint));
        }
        if (mountPoint == "swap") {
            const int index = newPartitionFsCombo_->findText("swap");
            if (index >= 0) {
                newPartitionFsCombo_->setCurrentIndex(index);
            }
        } else if (mountPoint == "/boot/efi") {
            const int index = newPartitionFsCombo_->findText("fat32");
            if (index >= 0) {
                newPartitionFsCombo_->setCurrentIndex(index);
            }
        } else if (newPartitionFsCombo_->currentText() == "swap") {
            const int index = newPartitionFsCombo_->findText("ext4");
            if (index >= 0) {
                newPartitionFsCombo_->setCurrentIndex(index);
            }
        }
    });

    const auto insertPlannedPartition = [this]() {
        if (!newPartitionAddButton_ || !newPartitionAddButton_->isEnabled()) {
            return;
        }
        addPartitionRow(newPartitionMountCombo_->currentText().trimmed(),
                        newPartitionLocalMountCombo_->currentText().trimmed(),
                        newPartitionFsCombo_->currentText().trimmed(),
                        newPartitionSizeSpin_->value(),
                        newPartitionFormatCheck_->isChecked());
        markInstallDirty();
    };

    connect(addAction, &QAction::triggered, this, insertPlannedPartition);
    connect(newMenuAction, &QAction::triggered, this, insertPlannedPartition);
    connect(newPartitionAddButton_, &QPushButton::clicked, this, insertPlannedPartition);
    connect(removeAction, &QAction::triggered, this, [this]() {
        const int row = partitionTable_->currentRow();
        if (row >= 0) {
            partitionTable_->removeRow(row);
            markInstallDirty();
        }
    });
    connect(deleteMenuAction, &QAction::triggered, this, [this]() {
        const int row = partitionTable_->currentRow();
        if (row >= 0) {
            partitionTable_->removeRow(row);
            markInstallDirty();
        }
    });
    connect(pendingOpsAction, &QAction::toggled, plannerStatus, &QWidget::setVisible);
    connect(partitionTable_, &QTableWidget::itemSelectionChanged, this, &InstallerWindow::refreshPartitionEditorPreview);

    return page;
}

QWidget *InstallerWindow::buildInstallPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(40, 10, 40, 24);
    layout->setSpacing(12);

    installStatusLabel_ = new QLabel("Current Step: Ready to start installation", page);
    installStatusLabel_->setMinimumHeight(33);
    installStatusLabel_->setStyleSheet("font-size: 15px; font-weight: 600;");
    installStatusLabel_->setWordWrap(true);
    layout->addWidget(installStatusLabel_);

    installProgressBar_ = new QProgressBar(page);
    installProgressBar_->setRange(0, 100);
    installProgressBar_->setValue(0);
    installProgressBar_->setFormat("%p%");
    installProgressBar_->setTextVisible(true);
    installProgressBar_->setMinimumHeight(35);
    layout->addWidget(installProgressBar_);

    installLog_ = new QPlainTextEdit(page);
    installLog_->setReadOnly(true);
    installLog_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    installLog_->setLineWrapMode(QPlainTextEdit::NoWrap);
    installLog_->setMinimumHeight(351);
    installLog_->setPlaceholderText("Install output will appear here.");
    installLog_->setFocusPolicy(Qt::NoFocus);
    installLog_->setStyleSheet(
        "QPlainTextEdit {"
        " background-color: #000000;"
        " color: #ffffff;"
        " border: 1px solid #1f1f1f;"
        " selection-background-color: #1e5aa8;"
        " selection-color: #ffffff;"
        " }"
    );
    layout->addWidget(installLog_, 1);

    auto *buttonRow = new QHBoxLayout();
    buttonRow->setContentsMargins(0, 0, 0, 0);
    pageThreeBackButton_ = new QPushButton("Back", page);
    pageThreeBackButton_->setFixedSize(150, 50);
    pageThreeInstallButton_ = new QPushButton("Install", page);
    pageThreeInstallButton_->setFixedSize(150, 50);
    buttonRow->addWidget(pageThreeBackButton_, 0, Qt::AlignLeft);
    buttonRow->addStretch(1);
    buttonRow->addWidget(pageThreeInstallButton_, 0, Qt::AlignRight);
    layout->addLayout(buttonRow);

    repoUrlEdit_ = new QLineEdit(page);
    repoUrlEdit_->setText("local-install-script");
    repoUrlEdit_->hide();
    repoBranchEdit_ = new QLineEdit(page);
    repoBranchEdit_->setText("main");
    repoBranchEdit_->hide();
    scriptPathEdit_ = new QLineEdit(page);
    scriptPathEdit_->setText("install.sh");
    scriptPathEdit_->hide();
    workRootEdit_ = new QLineEdit(page);
    workRootEdit_->setText("/tmp/lfs-installer");
    workRootEdit_->hide();

    connect(pageThreeBackButton_, &QPushButton::clicked, this, &InstallerWindow::handleBackAction);
    connect(pageThreeInstallButton_, &QPushButton::clicked, this, &InstallerWindow::handlePrimaryAction);

    return page;
}

QWidget *InstallerWindow::buildFeaturesPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QGridLayout(page);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setHorizontalSpacing(8);
    layout->setVerticalSpacing(8);

    auto *searchRow = new QHBoxLayout();
    searchRow->setContentsMargins(0, 0, 0, 0);
    searchRow->addStretch(1);
    featureSearchEdit_ = new QLineEdit(page);
    featureSearchEdit_->setPlaceholderText("Search");
    featureSearchEdit_->setFixedSize(300, 40);
    searchRow->addWidget(featureSearchEdit_);
    layout->addLayout(searchRow, 0, 0, 1, 5);

    featureListWidget_ = new QListWidget(page);
    featureListWidget_->setFocusPolicy(Qt::NoFocus);
    layout->addWidget(featureListWidget_, 1, 0, 1, 5);

    auto *installButton = new QPushButton("Install", page);
    auto *updateButton = new QPushButton("Update", page);
    auto *reloadButton = new QPushButton("Reload", page);
    auto *updatePackagesButton = new QPushButton("Update Packages", page);
    featureOutdatedButton_ = new QPushButton("Outdated Packages", page);

    installButton->setFocusPolicy(Qt::NoFocus);
    updateButton->setFocusPolicy(Qt::NoFocus);
    reloadButton->setFocusPolicy(Qt::NoFocus);
    updatePackagesButton->setFocusPolicy(Qt::NoFocus);
    featureOutdatedButton_->setFocusPolicy(Qt::NoFocus);
    layout->addWidget(installButton, 2, 0);
    layout->addWidget(updateButton, 2, 1);
    layout->addWidget(reloadButton, 2, 2);
    layout->addWidget(updatePackagesButton, 2, 3);
    layout->addWidget(featureOutdatedButton_, 2, 4);

    featureOutput_ = new QPlainTextEdit(page);
    featureOutput_->setReadOnly(true);
    featureOutput_->setFocusPolicy(Qt::NoFocus);
    featureOutput_->setLineWrapMode(QPlainTextEdit::NoWrap);
    featureOutput_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    layout->addWidget(featureOutput_, 3, 0, 1, 5);

    featureCountLabel_ = new QLabel("Packages loaded: 0 | Selected: 0", page);
    layout->addWidget(featureCountLabel_, 4, 0, 1, 5);

    layout->setRowStretch(1, 3);
    layout->setRowStretch(3, 2);

    if (!featureRepoManager_) {
        featureRepoManager_ = new QNetworkAccessManager(this);
        connect(featureRepoManager_, &QNetworkAccessManager::finished, this, &InstallerWindow::handleFeatureRepoReply);
    }

    connect(featureSearchEdit_, &QLineEdit::textChanged, this, [this](const QString &) {
        applyFeatureFilters();
        refreshSummaries();
    });
    connect(featureListWidget_, &QListWidget::itemChanged, this, [this]() {
        refreshSummaries();
    });
    connect(featureListWidget_, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *, QListWidgetItem *) {
        requestCurrentFeatureMetadata();
        refreshSummaries();
    });
    connect(reloadButton, &QPushButton::clicked, this, [this]() {
        if (featureSearchEdit_) {
            featureSearchEdit_->clear();
        }
        loadFeaturePackagesFromRepo();
    });
    connect(installButton, &QPushButton::clicked, this, [this]() {
        if (!featureOutput_) {
            return;
        }

        QStringList lines;
        lines << "Install action is not wired on page 4 yet.";
        lines << "";
        lines << "Selected repo packages:";
        const QStringList features = collectSelectedFeatures();
        if (features.isEmpty()) {
            lines << "  none";
        } else {
            for (const QString &feature : features) {
                lines << QString("  - %1").arg(feature);
            }
        }
        featureOutput_->setPlainText(lines.join('\n'));
    });
    connect(updateButton, &QPushButton::clicked, this, [this]() {
        if (featureOutput_) {
            featureOutput_->setPlainText("Update action is not wired on page 4 yet.\n\nUse Reload to refresh the GitHub-backed package list.");
        }
    });
    connect(updatePackagesButton, &QPushButton::clicked, this, [this]() {
        if (featureOutput_) {
            featureOutput_->setPlainText("Update Packages is not wired on page 4 yet.\n\nThis page currently mirrors the GitHub repo list and search flow only.");
        }
    });
    connect(featureOutdatedButton_, &QPushButton::clicked, this, [this]() {
        if (featureOutput_) {
            featureOutput_->setPlainText("Outdated package filtering is not wired on page 4 yet.\n\nThe repo browser is currently focused on package listing, search, and selection.");
        }
    });

    populateFeaturePackages();

    return page;
}

void InstallerWindow::addPartitionRow(const QString &mountPoint,
                                      const QString &localMountPoint,
                                      const QString &fileSystem,
                                      double sizeGiB,
                                      bool format)
{
    const int row = partitionTable_->rowCount();
    partitionTable_->insertRow(row);

    auto *partitionItem = new QTableWidgetItem();
    partitionItem->setFlags(partitionItem->flags() & ~Qt::ItemIsEditable);
    partitionTable_->setItem(row, 0, partitionItem);

    auto *mountCombo = new QComboBox(partitionTable_);
    mountCombo->setEditable(true);
    mountCombo->addItems({"/boot/efi", "/boot", "/", "/home", "/var", "swap"});
    mountCombo->setCurrentText(mountPoint);
    mountCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto *localMountCombo = new QComboBox(partitionTable_);
    localMountCombo->setEditable(true);
    localMountCombo->addItems(localMountPointOptions());
    localMountCombo->setCurrentText(localMountPoint.isEmpty() ? defaultLocalMountPoint(mountPoint) : localMountPoint);
    localMountCombo->setMinimumContentsLength(14);
    localMountCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    connect(localMountCombo, &QComboBox::currentTextChanged, this, &InstallerWindow::markInstallDirty);
    connect(mountCombo, &QComboBox::currentTextChanged, this, [this, localMountCombo](const QString &text) {
        localMountCombo->setCurrentText(defaultLocalMountPoint(text));
        markInstallDirty();
    });
    partitionTable_->setCellWidget(row, 2, mountCombo);
    partitionTable_->setCellWidget(row, 3, localMountCombo);

    auto *fsCombo = new QComboBox(partitionTable_);
    fsCombo->addItems({"ext4", "xfs", "btrfs", "fat32", "swap"});
    const int fsIndex = fsCombo->findText(fileSystem);
    fsCombo->setCurrentIndex(fsIndex >= 0 ? fsIndex : 0);
    connect(fsCombo, &QComboBox::currentTextChanged, this, [this](const QString &) {
        markInstallDirty();
    });
    partitionTable_->setCellWidget(row, 1, fsCombo);

    auto *labelItem = new QTableWidgetItem();
    labelItem->setFlags(labelItem->flags() & ~Qt::ItemIsEditable);
    partitionTable_->setItem(row, 4, labelItem);

    auto *sizeSpin = new QDoubleSpinBox(partitionTable_);
    sizeSpin->setRange(0.1, 102400.0);
    sizeSpin->setDecimals(1);
    sizeSpin->setSuffix(" GiB");
    sizeSpin->setValue(sizeGiB);
    connect(sizeSpin, &QDoubleSpinBox::valueChanged, this, [this](double) {
        markInstallDirty();
    });
    partitionTable_->setCellWidget(row, 5, sizeSpin);

    auto *usedItem = new QTableWidgetItem();
    usedItem->setFlags(usedItem->flags() & ~Qt::ItemIsEditable);
    partitionTable_->setItem(row, 6, usedItem);

    auto *unusedItem = new QTableWidgetItem();
    unusedItem->setFlags(unusedItem->flags() & ~Qt::ItemIsEditable);
    partitionTable_->setItem(row, 7, unusedItem);

    auto *formatCheck = new QCheckBox(partitionTable_);
    formatCheck->setChecked(format);
    PlannedPartition partition;
    partition.mountPoint = mountPoint;
    partition.localMountPoint = localMountCombo->currentText().trimmed();
    partition.fileSystem = fileSystem;
    partition.sizeGiB = sizeGiB;
    partition.format = format;
    formatCheck->setText(flagsText(partition));
    connect(formatCheck, &QCheckBox::checkStateChanged, this, &InstallerWindow::markInstallDirty);
    partitionTable_->setCellWidget(row, 8, formatCheck);
}

QVector<PlannedPartition> InstallerWindow::collectPartitions() const
{
    QVector<PlannedPartition> partitions;
    for (int row = 0; row < partitionTable_->rowCount(); ++row) {
        auto *mountCombo = qobject_cast<QComboBox *>(partitionTable_->cellWidget(row, 2));
        auto *localMountCombo = qobject_cast<QComboBox *>(partitionTable_->cellWidget(row, 3));
        auto *fsCombo = qobject_cast<QComboBox *>(partitionTable_->cellWidget(row, 1));
        auto *sizeSpin = qobject_cast<QDoubleSpinBox *>(partitionTable_->cellWidget(row, 5));
        auto *formatCheck = qobject_cast<QCheckBox *>(partitionTable_->cellWidget(row, 8));

        if (!mountCombo || !localMountCombo || !fsCombo || !sizeSpin || !formatCheck) {
            continue;
        }

        PlannedPartition partition;
        partition.mountPoint = mountCombo->currentText().trimmed();
        partition.localMountPoint = localMountCombo->currentText().trimmed();
        partition.fileSystem = fsCombo->currentText().trimmed();
        partition.sizeGiB = sizeSpin->value();
        partition.format = formatCheck->isChecked();
        partitions.append(partition);
    }

    return partitions;
}

void InstallerWindow::refreshPartitionEditorPreview()
{
    if (!partitionTable_ || !partitionMapWidget_ || !partitionCapacityLabel_ || !partitionOperationsLabel_) {
        return;
    }

    const QVector<PlannedPartition> partitions = collectPartitions();
    const DriveInfo drive = currentDrive();
    const double driveGiB = bytesToGiB(drive.sizeBytes);
    double plannedGiB = 0.0;
    for (const PlannedPartition &partition : partitions) {
        plannedGiB += partition.sizeGiB;
    }
    const double unallocatedGiB = driveGiB > plannedGiB ? driveGiB - plannedGiB : 0.0;

    for (int row = 0; row < partitionTable_->rowCount(); ++row) {
        auto *sizeSpin = qobject_cast<QDoubleSpinBox *>(partitionTable_->cellWidget(row, 5));
        if (!sizeSpin) {
            continue;
        }

        if (drive.path.isEmpty()) {
            sizeSpin->setMaximum(102400.0);
            continue;
        }

        const double currentSize = sizeSpin->value();
        const double otherPlannedGiB = plannedGiB - currentSize;
        const double rowMaxGiB = qMax(currentSize, driveGiB - otherPlannedGiB);
        sizeSpin->setMaximum(qMax(0.1, rowMaxGiB));
    }

    if (newPartitionSizeSpin_) {
        if (drive.path.isEmpty()) {
            newPartitionSizeSpin_->setMaximum(102400.0);
            if (newPartitionRemainingLabel_) {
                newPartitionRemainingLabel_->setText("Remaining: select a drive");
            }
            if (newPartitionAddButton_) {
                newPartitionAddButton_->setEnabled(false);
            }
        } else {
            const double availableGiB = qMax(0.0, unallocatedGiB);
            newPartitionSizeSpin_->setMaximum(qMax(0.1, availableGiB));
            if (newPartitionSizeSpin_->value() > availableGiB && availableGiB >= 0.1) {
                newPartitionSizeSpin_->setValue(availableGiB);
            }
            if (newPartitionRemainingLabel_) {
                newPartitionRemainingLabel_->setText(
                    QString("Remaining: %1 GiB").arg(QString::number(availableGiB, 'f', 2)));
            }
            if (newPartitionAddButton_) {
                newPartitionAddButton_->setEnabled(availableGiB >= 0.1);
            }
        }
    }

    for (int row = 0; row < partitions.size(); ++row) {
        if (auto *partitionItem = partitionTable_->item(row, 0)) {
            partitionItem->setText(partitionNodeName(drive.path, row));
        }
        if (auto *labelItem = partitionTable_->item(row, 4)) {
            labelItem->setText(partitionLabelText(partitions.at(row).mountPoint));
        }
        if (auto *usedItem = partitionTable_->item(row, 6)) {
            usedItem->setText(usedText(partitions.at(row)));
        }
        if (auto *unusedItem = partitionTable_->item(row, 7)) {
            unusedItem->setText(unusedText(partitions.at(row)));
        }
        if (auto *formatCheck = qobject_cast<QCheckBox *>(partitionTable_->cellWidget(row, 8))) {
            formatCheck->setText(flagsText(partitions.at(row)));
        }
    }

    if (drive.path.isEmpty()) {
        partitionCapacityLabel_->setText(QString("Planned: %1 GiB").arg(QString::number(plannedGiB, 'f', 2)));
    } else if (plannedGiB <= driveGiB) {
        partitionCapacityLabel_->setText(
            QString("Drive: %1 GiB   Planned: %2 GiB   Remaining: %3 GiB")
                .arg(QString::number(driveGiB, 'f', 2),
                     QString::number(plannedGiB, 'f', 2),
                     QString::number(unallocatedGiB, 'f', 2)));
    } else {
        partitionCapacityLabel_->setText(
            QString("Drive: %1 GiB   Planned: %2 GiB   Over by: %3 GiB")
                .arg(QString::number(driveGiB, 'f', 2),
                     QString::number(plannedGiB, 'f', 2),
                     QString::number(plannedGiB - driveGiB, 'f', 2)));
    }

    QStringList operations;
    for (int row = 0; row < partitions.size(); ++row) {
        operations.append(QString("%1 %2 -> %3 (%4)")
                              .arg(partitionNodeName(drive.path, row),
                                   partitions.at(row).mountPoint,
                                   partitions.at(row).localMountPoint,
                                   flagsText(partitions.at(row))));
    }
    partitionOperationsLabel_->setText(QString("%1 operation%2 pending")
                                           .arg(partitions.size())
                                           .arg(partitions.size() == 1 ? "" : "s"));
    partitionOperationsLabel_->setToolTip(operations.join("\n"));

    auto *mapLayout = qobject_cast<QHBoxLayout *>(partitionMapWidget_->layout());
    while (QLayoutItem *item = mapLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    if (partitions.isEmpty()) {
        auto *emptyLabel = new QLabel("No partitions planned", partitionMapWidget_);
        emptyLabel->setAlignment(Qt::AlignCenter);
        emptyLabel->setFrameShape(QFrame::StyledPanel);
        emptyLabel->setMinimumHeight(56);
        mapLayout->addWidget(emptyLabel);
        return;
    }

    const int selectedRow = partitionTable_->currentRow();
    for (int row = 0; row < partitions.size(); ++row) {
        const PlannedPartition &partition = partitions.at(row);
        auto *segment = new QToolButton(partitionMapWidget_);
        segment->setToolButtonStyle(Qt::ToolButtonTextOnly);
        segment->setCheckable(true);
        segment->setChecked(row == selectedRow);
        segment->setAutoRaise(false);
        segment->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        segment->setMinimumWidth(88);
        segment->setMinimumHeight(56);
        segment->setText(QString("%1\n%2 GiB").arg(partitionLabelText(partition.mountPoint),
                                                   QString::number(partition.sizeGiB, 'f', 2)));
        segment->setToolTip(QString("%1\n%2\n%3\n%4\n%5")
                                .arg(partitionNodeName(drive.path, row),
                                     partition.fileSystem,
                                     partition.mountPoint,
                                     partition.localMountPoint,
                                     flagsText(partition)));
        connect(segment, &QToolButton::clicked, this, [this, row]() {
            partitionTable_->selectRow(row);
        });

        int stretch = static_cast<int>(partition.sizeGiB * 10.0);
        if (stretch < 1) {
            stretch = 1;
        }
        mapLayout->addWidget(segment, stretch);
    }

    if (unallocatedGiB > 0.05) {
        auto *segment = new QToolButton(partitionMapWidget_);
        segment->setToolButtonStyle(Qt::ToolButtonTextOnly);
        segment->setEnabled(false);
        segment->setMinimumWidth(88);
        segment->setMinimumHeight(56);
        segment->setText(QString("Unallocated\n%1 GiB").arg(QString::number(unallocatedGiB, 'f', 2)));

        int stretch = static_cast<int>(unallocatedGiB * 10.0);
        if (stretch < 1) {
            stretch = 1;
        }
        mapLayout->addWidget(segment, stretch);
    }
}

void InstallerWindow::populateFeaturePackages()
{
    loadFeaturePackagesFromRepo();
}

void InstallerWindow::applyFeatureFilters()
{
    if (!featureListWidget_) {
        return;
    }

    const QString term = featureSearchEdit_ ? featureSearchEdit_->text().trimmed() : QString();
    QListWidgetItem *firstVisibleItem = nullptr;
    for (int row = 0; row < featureListWidget_->count(); ++row) {
        QListWidgetItem *item = featureListWidget_->item(row);
        if (!item) {
            continue;
        }

        const bool matchesSearch = term.isEmpty() || item->text().contains(term, Qt::CaseInsensitive);
        item->setHidden(!matchesSearch);
        if (!item->isHidden() && !firstVisibleItem) {
            firstVisibleItem = item;
        }
    }

    if (featureListWidget_->currentItem() && !featureListWidget_->currentItem()->isHidden()) {
        return;
    }

    if (firstVisibleItem) {
        featureListWidget_->setCurrentItem(firstVisibleItem);
    } else {
        featureListWidget_->setCurrentRow(-1);
    }
}

void InstallerWindow::loadFeaturePackagesFromRepo()
{
    if (!featureRepoManager_ || !featureListWidget_) {
        return;
    }

    ++featureRepoRequestGeneration_;
    activeFeatureMetadataRequests_ = 0;
    const auto inFlightReplies = featureRepoManager_->findChildren<QNetworkReply *>();
    for (QNetworkReply *inFlightReply : inFlightReplies) {
        inFlightReply->abort();
    }

    {
        QSignalBlocker blocker(featureListWidget_);
        featureListWidget_->clear();
    }

    if (featureOutput_) {
        featureOutput_->setPlainText("Loading package index from GitHub...\n\nSource: voncloft/Voncloft-OS/REPOS");
    }
    if (featureCountLabel_) {
        featureCountLabel_->setText("Packages loaded: loading... | Selected: 0");
    }

    QNetworkRequest request{QUrl{githubReposTreeUrl()}};
    request.setHeader(QNetworkRequest::UserAgentHeader, "lfs-installer");
    request.setRawHeader("Accept", "application/vnd.github+json");
    QNetworkReply *reply = featureRepoManager_->get(request);
    reply->setProperty("requestType", "index");
    reply->setProperty("generation", featureRepoRequestGeneration_);
}

void InstallerWindow::queueFeatureMetadataRequests()
{
    if (!featureListWidget_) {
        return;
    }

    while (activeFeatureMetadataRequests_ < MaxConcurrentFeatureMetadataRequests) {
        QListWidgetItem *nextItem = nullptr;
        for (int row = 0; row < featureListWidget_->count(); ++row) {
            QListWidgetItem *candidate = featureListWidget_->item(row);
            if (!candidate) {
                continue;
            }
            if (candidate->data(FeatureMetadataLoadedRole).toBool() || candidate->data(FeatureMetadataLoadingRole).toBool()) {
                continue;
            }
            nextItem = candidate;
            break;
        }

        if (!nextItem) {
            return;
        }

        requestFeatureMetadataForItem(nextItem);
    }
}

void InstallerWindow::requestFeatureMetadataForItem(QListWidgetItem *item)
{
    if (!featureRepoManager_ || !item) {
        return;
    }

    if (item->data(FeatureMetadataLoadedRole).toBool() || item->data(FeatureMetadataLoadingRole).toBool()) {
        return;
    }

    const QString rawUrl = item->data(FeatureRawUrlRole).toString();
    if (rawUrl.isEmpty()) {
        return;
    }

    item->setData(FeatureMetadataLoadingRole, true);
    ++activeFeatureMetadataRequests_;

    if (item == featureListWidget_->currentItem()) {
        refreshSummaries();
    }

    QNetworkRequest request{QUrl{rawUrl}};
    request.setHeader(QNetworkRequest::UserAgentHeader, "lfs-installer");
    QNetworkReply *reply = featureRepoManager_->get(request);
    reply->setProperty("requestType", "metadata");
    reply->setProperty("generation", featureRepoRequestGeneration_);
    reply->setProperty("repoPath", item->data(FeatureRepoPathRole).toString());
    reply->setProperty("rawUrl", rawUrl);
}

void InstallerWindow::requestCurrentFeatureMetadata()
{
    if (!featureListWidget_) {
        return;
    }

    QListWidgetItem *item = featureListWidget_->currentItem();
    if (!item) {
        return;
    }
    requestFeatureMetadataForItem(item);
}

void InstallerWindow::handleFeatureRepoReply(QNetworkReply *reply)
{
    if (!reply) {
        return;
    }

    const QString requestType = reply->property("requestType").toString();
    const int generation = reply->property("generation").toInt();
    const QByteArray payload = reply->readAll();
    const bool requestSucceeded = reply->error() == QNetworkReply::NoError;
    const QString errorText = reply->errorString();
    reply->deleteLater();

    if (generation != featureRepoRequestGeneration_) {
        return;
    }

    if (requestType == "index") {
        if (!requestSucceeded) {
            if (featureOutput_) {
                featureOutput_->setPlainText(QString("Failed to load repo index from GitHub.\n\n%1").arg(errorText));
            }
            if (featureCountLabel_) {
                featureCountLabel_->setText("Packages loaded: 0 | Selected: 0");
            }
            return;
        }

        const QJsonDocument document = QJsonDocument::fromJson(payload);
        const QJsonArray tree = document.object().value("tree").toArray();

        QSignalBlocker blocker(featureListWidget_);
        featureListWidget_->clear();

        for (const QJsonValue &value : tree) {
            const QJsonObject object = value.toObject();
            const QString path = object.value("path").toString();
            if (!path.startsWith("REPOS/") || !path.endsWith("/spkgbuild")) {
                continue;
            }

            const QStringList parts = path.split('/');
            if (parts.size() < 4) {
                continue;
            }

            const QString category = parts.mid(1, parts.size() - 3).join('/');
            const QString packageName = parts.at(parts.size() - 2);
            const QString packageKey = QString("%1/%2").arg(category, packageName);
            const QString rawUrl = githubRawPrefix() + path;

            auto *item = new QListWidgetItem(featureDisplayText(category, packageName, QString(), QString()), featureListWidget_);
            item->setFlags((item->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsEditable);
            item->setCheckState(Qt::Unchecked);
            item->setData(FeatureKeyRole, packageKey);
            item->setData(FeatureVersionRole, QString());
            item->setData(FeatureDescriptionRole, QString());
            item->setData(FeatureRawUrlRole, rawUrl);
            item->setData(FeatureMetadataLoadedRole, false);
            item->setData(FeatureMetadataLoadingRole, false);
            item->setData(FeatureRepoPathRole, path);
            item->setToolTip(path);
        }

        featureListWidget_->sortItems(Qt::AscendingOrder);
        applyFeatureFilters();
        if (featureListWidget_->count() > 0 && !featureListWidget_->currentItem()) {
            featureListWidget_->setCurrentRow(0);
        }
        queueFeatureMetadataRequests();
        refreshSummaries();
        return;
    }

    if (requestType != "metadata" || !featureListWidget_) {
        return;
    }

    if (activeFeatureMetadataRequests_ > 0) {
        --activeFeatureMetadataRequests_;
    }

    const QString rawUrl = reply->property("rawUrl").toString();
    for (int row = 0; row < featureListWidget_->count(); ++row) {
        QListWidgetItem *item = featureListWidget_->item(row);
        if (!item || item->data(FeatureRawUrlRole).toString() != rawUrl) {
            continue;
        }

        QString version;
        QString description;
        if (requestSucceeded) {
            const QString contents = QString::fromUtf8(payload);
            const QRegularExpression versionPattern(R"(^\s*version=(.+)\s*$)", QRegularExpression::MultilineOption);
            const QRegularExpression descriptionPattern(R"(^\s*#\s*description\s*:\s*(.+)\s*$)", QRegularExpression::MultilineOption);
            version = versionPattern.match(contents).captured(1).trimmed();
            description = descriptionPattern.match(contents).captured(1).trimmed();
            if ((version.startsWith('"') && version.endsWith('"')) || (version.startsWith('\'') && version.endsWith('\''))) {
                version = version.mid(1, version.size() - 2);
            }
        } else {
            description = QString("Metadata unavailable (%1)").arg(errorText);
        }

        const QString packageKey = item->data(FeatureKeyRole).toString();
        const int slashIndex = packageKey.lastIndexOf('/');
        const QString category = slashIndex >= 0 ? packageKey.left(slashIndex) : QString();
        const QString packageName = slashIndex >= 0 ? packageKey.mid(slashIndex + 1) : packageKey;

        {
            QSignalBlocker blocker(featureListWidget_);
            item->setData(FeatureVersionRole, version);
            item->setData(FeatureDescriptionRole, description);
            item->setData(FeatureMetadataLoadedRole, true);
            item->setData(FeatureMetadataLoadingRole, false);
            item->setText(featureDisplayText(category, packageName, version, description));
        }

        if (featureSearchEdit_ && !featureSearchEdit_->text().trimmed().isEmpty()) {
            applyFeatureFilters();
        }
        if (item == featureListWidget_->currentItem()) {
            refreshSummaries();
        }
        break;
    }

    queueFeatureMetadataRequests();
}

QStringList InstallerWindow::collectSelectedFeatures() const
{
    QStringList features;
    if (!featureListWidget_) {
        return features;
    }

    for (int row = 0; row < featureListWidget_->count(); ++row) {
        QListWidgetItem *item = featureListWidget_->item(row);
        if (item && item->checkState() == Qt::Checked) {
            features.append(item->data(FeatureKeyRole).toString());
        }
    }

    return features;
}

QString InstallerWindow::buildFeatureOutputText() const
{
    QStringList lines;
    lines << "Source: https://github.com/voncloft/Voncloft-OS/tree/master/REPOS";

    int visibleCount = 0;
    const int totalCount = featureListWidget_ ? featureListWidget_->count() : 0;
    for (int row = 0; row < totalCount; ++row) {
        QListWidgetItem *item = featureListWidget_->item(row);
        if (item && !item->isHidden()) {
            ++visibleCount;
        }
    }

    if (totalCount == 0) {
        lines << "";
        lines << "Package index: waiting for GitHub reply...";
    }

    if (featureListWidget_ && featureListWidget_->currentItem()) {
        QListWidgetItem *item = featureListWidget_->currentItem();
        const QString packageKey = item->data(FeatureKeyRole).toString();
        const int slashIndex = packageKey.lastIndexOf('/');
        const QString category = slashIndex >= 0 ? packageKey.left(slashIndex) : QString();
        const QString packageName = slashIndex >= 0 ? packageKey.mid(slashIndex + 1) : packageKey;

        lines << "";
        lines << QString("Package: %1").arg(packageName);
        lines << QString("Category: %1").arg(category);
        if (item->data(FeatureMetadataLoadingRole).toBool()) {
            lines << "Metadata: loading from GitHub...";
        } else {
            const QString version = item->data(FeatureVersionRole).toString().trimmed();
            const QString description = item->data(FeatureDescriptionRole).toString().trimmed();
            lines << QString("Version: %1").arg(version.isEmpty() ? QStringLiteral("(not loaded)") : version);
            lines << QString("Description: %1").arg(description.isEmpty() ? QStringLiteral("(not loaded)") : description);
        }
        lines << QString("Repo path: %1").arg(item->data(FeatureRepoPathRole).toString());
        lines << QString("Selected: %1").arg(item->checkState() == Qt::Checked ? "yes" : "no");
    }

    lines << "";
    lines << QString("Visible packages: %1 of %2").arg(visibleCount).arg(totalCount);
    if (featureSearchEdit_ && !featureSearchEdit_->text().trimmed().isEmpty()) {
        lines << QString("Search: %1").arg(featureSearchEdit_->text().trimmed());
    }

    const QStringList selectedFeatures = collectSelectedFeatures();
    lines << QString("Selected packages: %1").arg(selectedFeatures.size());
    if (selectedFeatures.isEmpty()) {
        lines << "Selections: none";
    } else {
        lines << "Selections:";
        for (const QString &feature : selectedFeatures) {
            lines << QString("  - %1").arg(feature);
        }
    }

    return lines.join('\n');
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
    QSet<QString> localMountPoints;
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
        if (partition.mountPoint != "swap") {
            if (partition.localMountPoint.isEmpty()) {
                QMessageBox::warning(this, "Invalid partition", QString("Choose a local mount point for `%1`.").arg(partition.mountPoint));
                return false;
            }
            if (localMountPoints.contains(partition.localMountPoint)) {
                QMessageBox::warning(this,
                                     "Duplicate local mount point",
                                     QString("The local mount point `%1` is defined more than once.").arg(partition.localMountPoint));
                return false;
            }
            localMountPoints.insert(partition.localMountPoint);
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
    if (deviceTree_) {
        deviceTree_->clear();
    }

    QProcess lsblk;
    lsblk.start("lsblk", {"--json", "-b", "-o", "NAME,SIZE,TYPE,MODEL,PATH,MOUNTPOINT,FSTYPE"});
    if (!lsblk.waitForFinished(4000) || lsblk.exitStatus() != QProcess::NormalExit || lsblk.exitCode() != 0) {
        driveDetailsLabel_->setText("Unable to query drives with `lsblk`.");
        refreshPartitionEditorPreview();
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(lsblk.readAllStandardOutput());
    const QJsonArray blockDevices = document.object().value("blockdevices").toArray();
    for (const QJsonValue &deviceValue : blockDevices) {
        const QJsonObject deviceObject = deviceValue.toObject();
        if (deviceTree_) {
            appendDeviceNode(deviceTree_->invisibleRootItem(), deviceObject);
        }

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

    if (deviceTree_) {
        deviceTree_->expandAll();
    }
    updateDriveDetails();
}

void InstallerWindow::updateDriveDetails()
{
    const DriveInfo drive = currentDrive();
    if (drive.path.isEmpty()) {
        driveDetailsLabel_->setText("Select a target disk, then create partitions manually. Remaining space updates as you size each entry.");
        refreshPartitionEditorPreview();
        return;
    }

    driveDetailsLabel_->setText(QString("Current device: %1. Define your partitions manually; remaining space is calculated automatically.")
                                    .arg(driveLabel(drive)));
    refreshPartitionEditorPreview();
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
    const QString runStamp = QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss");
    QDir directory;
    if (!directory.mkpath(workRoot)) {
        QMessageBox::critical(this, "Working directory error", QString("Unable to create `%1`.").arg(workRoot));
        return;
    }

    currentRunDirectory_ = QDir(workRoot).filePath("run-" + runStamp);
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

    const QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    if (desktopPath.isEmpty()) {
        QMessageBox::critical(this, "Desktop path error", "Unable to locate the Desktop directory.");
        return;
    }

    const QString desktopSummaryPath = QDir(desktopPath).filePath("lfs-installer-setup-" + runStamp + ".txt");
    QSaveFile summaryFile(desktopSummaryPath);
    if (!summaryFile.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, "Summary write failed", QString("Unable to write `%1`.").arg(desktopSummaryPath));
        return;
    }
    summaryFile.write(buildConfigText().toUtf8());
    if (!summaryFile.commit()) {
        QMessageBox::critical(this, "Summary write failed", QString("Failed to finalize `%1`.").arg(desktopSummaryPath));
        return;
    }

    const QString scriptsDirectory = findScriptsDirectory();
    if (scriptsDirectory.isEmpty()) {
        QMessageBox::critical(this, "Scripts folder missing", "Unable to locate the project's `scripts` folder.");
        return;
    }

    QString runtimeScriptsDirectory;
    QString artifactError;
    if (!generateInstallArtifacts(scriptsDirectory, &runtimeScriptsDirectory, &artifactError)) {
        QMessageBox::critical(this, "Artifact generation failed", artifactError);
        return;
    }

    installScriptPaths_ = collectScriptPaths(runtimeScriptsDirectory);
    if (installScriptPaths_.isEmpty()) {
        QMessageBox::critical(this, "Install list empty", "No scripts were listed in `scripts/install.sh`.");
        return;
    }

    if (!installProcess_) {
        installProcess_ = new QProcess(this);
        installProcess_->setProcessChannelMode(QProcess::MergedChannels);
        connect(installProcess_, &QProcess::readyRead, this, &InstallerWindow::handleInstallProcessOutput);
        connect(installProcess_, &QProcess::finished, this, &InstallerWindow::handleInstallProcessFinished);
        connect(installProcess_, &QProcess::errorOccurred, this, &InstallerWindow::handleInstallProcessError);
    }

    installOutputBuffer_.clear();
    pendingInstallStepText_.clear();
    currentInstallScriptPath_.clear();
    currentInstallScriptIndex_ = 0;
    completedInstallSteps_ = 0;
    totalInstallSteps_ = installScriptPaths_.size();
    installInProgress_ = true;
    installCompleted_ = false;

    installLog_->clear();
    if (installProgressBar_) {
        if (totalInstallSteps_ > 0) {
            installProgressBar_->setRange(0, totalInstallSteps_);
            installProgressBar_->setValue(0);
        } else {
            installProgressBar_->setRange(0, 0);
        }
    }
    appendInstallLogLine("$ prepare run directory");
    appendInstallLogLine(QString("> %1").arg(currentRunDirectory_));
    appendInstallLogLine("$ write install-config.json");
    appendInstallLogLine("$ write desktop setup summary");
    appendInstallLogLine(QString("> %1").arg(desktopSummaryPath));
    appendInstallLogLine("$ generate install artifacts");
    appendInstallLogLine(QString("> %1").arg(QDir(runtimeScriptsDirectory).filePath("final_setup.sh")));
    appendInstallLogLine(QString("> %1").arg(QDir(runtimeScriptsDirectory).filePath("partition.sh")));
    appendInstallLogLine(QString("> %1").arg(QDir(runtimeScriptsDirectory).filePath("mount.sh")));
    const QString filesDirectory = QDir(QFileInfo(runtimeScriptsDirectory).absolutePath()).filePath("files");
    appendInstallLogLine(QString("> %1").arg(QDir(filesDirectory).filePath("hostname")));
    appendInstallLogLine(QString("> %1").arg(QDir(filesDirectory).filePath("clock")));
    appendInstallLogLine(QString("> %1").arg(QDir(filesDirectory).filePath("fstab")));
    appendInstallLogLine("$ read scripts/install.sh");
    appendInstallLogLine(QString("> %1").arg(QDir(runtimeScriptsDirectory).filePath("install.sh")));
    appendInstallLogLine(QString("> source scripts dir %1").arg(scriptsDirectory));
    appendInstallLogLine(QString("> using scripts dir %1").arg(runtimeScriptsDirectory));
    for (const QString &scriptPath : installScriptPaths_) {
        appendInstallLogLine(QString("> queued %1").arg(scriptPath));
    }
    setInstallStatus("Current Step: Starting", QColor("#1b5e20"));
    updateNavigationState();
    startNextInstallScript();
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
    refreshPartitionEditorPreview();

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
    if (refreshingSummaries_) {
        return;
    }

    refreshingSummaries_ = true;
    const QString summaryText = QString::fromUtf8(buildConfigJson(true));

    if (configPreview_ && configPreview_->toPlainText() != summaryText) {
        configPreview_->setPlainText(summaryText);
    }
    if (featureOutput_) {
        const QString featureText = buildFeatureOutputText();
        if (featureOutput_->toPlainText() != featureText) {
            featureOutput_->setPlainText(featureText);
        }
    }
    if (featureCountLabel_) {
        const int loadedCount = featureListWidget_ ? featureListWidget_->count() : 0;
        const int selectedCount = collectSelectedFeatures().size();
        featureCountLabel_->setText(QString("Packages loaded: %1 | Selected: %2").arg(loadedCount).arg(selectedCount));
    }

    refreshingSummaries_ = false;
}

void InstallerWindow::updateNavigationState()
{
    const int pageIndex = pages_->currentIndex();
    backButton_->setEnabled(pageIndex > 0 && !installInProgress_);
    backButton_->setVisible(pageIndex != 2);

    primaryButton_->setEnabled(!installInProgress_);
    primaryButton_->setVisible(pageIndex != 2);

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

    if (pageThreeBackButton_) {
        pageThreeBackButton_->setEnabled(pageIndex > 0 && !installInProgress_);
        pageThreeBackButton_->setVisible(pageIndex == 2);
    }
    if (pageThreeInstallButton_) {
        pageThreeInstallButton_->setEnabled(!installInProgress_);
        pageThreeInstallButton_->setVisible(pageIndex == 2);
        if (pageIndex == 2) {
            pageThreeInstallButton_->setText(installInProgress_ ? "Installing..." : (installCompleted_ ? "Next" : "Install"));
        }
    }
}

void InstallerWindow::setInstallStatus(const QString &message, const QColor &color)
{
    installStatusLabel_->setText(message);
    installStatusLabel_->setStyleSheet(QString("color: %1; font-family: monospace; font-size: 14px;").arg(color.name()));
}

void InstallerWindow::resetInstallState(const QString &reason)
{
    installCompleted_ = false;
    pendingInstallStepText_.clear();
    currentInstallScriptPath_.clear();
    if (installProgressBar_) {
        installProgressBar_->setValue(0);
    }
    if (!reason.isEmpty()) {
        setInstallStatus(QString("Current Step: %1").arg(reason), QColor("#111111"));
    } else if (installStatusLabel_) {
        setInstallStatus("Current Step: Ready to start installation", QColor("#111111"));
    }
}

QString InstallerWindow::findScriptsDirectory() const
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir(appDir).filePath("../scripts"),
        QDir(appDir).filePath("scripts"),
        QDir::current().filePath("scripts")
    };

    for (const QString &candidate : candidates) {
        if (QFileInfo(candidate).isDir()) {
            return QDir(candidate).absolutePath();
        }
    }

    return {};
}

bool InstallerWindow::generateInstallArtifacts(const QString &sourceScriptsDirectory,
                                               QString *runtimeScriptsDirectory,
                                               QString *errorMessage) const
{
    if (currentRunDirectory_.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("No run directory has been prepared yet.");
        }
        return false;
    }

    const QString projectRoot = QFileInfo(sourceScriptsDirectory).absolutePath();
    const QString targetScriptsDirectory = sourceScriptsDirectory;
    const QString targetFilesDirectory = QDir(projectRoot).filePath("files");
    const QString stagedRoot = QDir(currentRunDirectory_).filePath("generated-artifacts");
    const QString stagedScriptsDirectory = QDir(stagedRoot).filePath("scripts");
    const QString stagedFilesDirectory = QDir(stagedRoot).filePath("files");
    const QString driverScriptPath = QDir(stagedRoot).filePath("install-driver.sh");

    QDir directory;
    if (!directory.mkpath(stagedScriptsDirectory)) {
        if (errorMessage) {
            *errorMessage = QString("Unable to create `%1`.").arg(stagedScriptsDirectory);
        }
        return false;
    }
    if (!directory.mkpath(stagedFilesDirectory)) {
        if (errorMessage) {
            *errorMessage = QString("Unable to create `%1`.").arg(stagedFilesDirectory);
        }
        return false;
    }

    const auto writeFile = [errorMessage](const QString &path, const QString &contents, bool executable) -> bool {
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            if (errorMessage) {
                *errorMessage = QString("Unable to open `%1` for writing.").arg(path);
            }
            return false;
        }

        QString normalized = contents;
        if (!normalized.endsWith('\n')) {
            normalized.append('\n');
        }

        if (file.write(normalized.toUtf8()) < 0) {
            if (errorMessage) {
                *errorMessage = QString("Unable to write `%1`.").arg(path);
            }
            return false;
        }

        if (!file.commit()) {
            if (errorMessage) {
                *errorMessage = QString("Failed to finalize `%1`.").arg(path);
            }
            return false;
        }

        const QFileDevice::Permissions permissions = executable
                                                         ? (QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
                                                            | QFileDevice::ReadGroup | QFileDevice::ExeGroup
                                                            | QFileDevice::ReadOther | QFileDevice::ExeOther)
                                                         : (QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                                            | QFileDevice::ReadGroup | QFileDevice::ReadOther);
        if (!QFile::setPermissions(path, permissions)) {
            if (errorMessage) {
                *errorMessage = QString("Unable to set permissions on `%1`.").arg(path);
            }
            return false;
        }

        return true;
    };

    QDirIterator scriptIterator(sourceScriptsDirectory,
                                QStringList() << "*.sh",
                                QDir::Files | QDir::Readable,
                                QDirIterator::Subdirectories);
    while (scriptIterator.hasNext()) {
        const QString sourcePath = scriptIterator.next();
        const QString relativePath = QDir(sourceScriptsDirectory).relativeFilePath(sourcePath);
        const QString stagedPath = QDir(stagedScriptsDirectory).filePath(relativePath);
        if (!directory.mkpath(QFileInfo(stagedPath).absolutePath())) {
            if (errorMessage) {
                *errorMessage = QString("Unable to create `%1`.").arg(QFileInfo(stagedPath).absolutePath());
            }
            return false;
        }

        QFile sourceFile(sourcePath);
        if (!sourceFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            if (errorMessage) {
                *errorMessage = QString("Unable to read `%1`.").arg(sourceFile.fileName());
            }
            return false;
        }

        if (!writeFile(stagedPath, QString::fromUtf8(sourceFile.readAll()), true)) {
            return false;
        }
    }

    const QDir sourceFilesDir(targetFilesDirectory);
    const QStringList existingFileNames = sourceFilesDir.entryList(QDir::Files | QDir::Readable, QDir::Name);
    for (const QString &fileName : existingFileNames) {
        QFile sourceFile(sourceFilesDir.filePath(fileName));
        if (!sourceFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            if (errorMessage) {
                *errorMessage = QString("Unable to read `%1`.").arg(sourceFile.fileName());
            }
            return false;
        }

        if (!writeFile(QDir(stagedFilesDirectory).filePath(fileName), QString::fromUtf8(sourceFile.readAll()), false)) {
            return false;
        }
    }

    if (!writeFile(QDir(stagedScriptsDirectory).filePath("final_setup.sh"), buildFinalSetupScript(), true)) {
        return false;
    }
    if (!writeFile(QDir(stagedScriptsDirectory).filePath("partition.sh"), buildPartitionScript(), true)) {
        return false;
    }
    if (!writeFile(QDir(stagedScriptsDirectory).filePath("mount.sh"), buildMountScript(), true)) {
        return false;
    }
    if (!writeFile(QDir(stagedFilesDirectory).filePath("hostname"), buildHostnameFile(), false)) {
        return false;
    }
    if (!writeFile(QDir(stagedFilesDirectory).filePath("clock"), buildClockFile(), false)) {
        return false;
    }
    if (!writeFile(QDir(stagedFilesDirectory).filePath("fstab"), buildFstabFile(), false)) {
        return false;
    }

    const QString driverScript =
        "#!/usr/bin/env bash\n"
        "set -euo pipefail\n"
        "STAGED_SCRIPTS=\"$(cd -- \"$(dirname -- \"$0\")/scripts\" && pwd)\"\n"
        "STAGED_FILES=\"$(cd -- \"$(dirname -- \"$0\")/files\" && pwd)\"\n"
        "TARGET_SCRIPTS=" + shellQuote(targetScriptsDirectory) + "\n"
        "TARGET_FILES=" + shellQuote(targetFilesDirectory) + "\n"
        "install -d \"$TARGET_SCRIPTS\" \"$TARGET_FILES\"\n"
        "install -m 755 \"$STAGED_SCRIPTS/final_setup.sh\" \"$TARGET_SCRIPTS/final_setup.sh\"\n"
        "install -m 755 \"$STAGED_SCRIPTS/partition.sh\" \"$TARGET_SCRIPTS/partition.sh\"\n"
        "install -m 755 \"$STAGED_SCRIPTS/mount.sh\" \"$TARGET_SCRIPTS/mount.sh\"\n"
        "install -m 644 \"$STAGED_FILES/hostname\" \"$TARGET_FILES/hostname\"\n"
        "install -m 644 \"$STAGED_FILES/clock\" \"$TARGET_FILES/clock\"\n"
        "install -m 644 \"$STAGED_FILES/fstab\" \"$TARGET_FILES/fstab\"\n"
        "while IFS= read -r script_name || [ -n \"$script_name\" ]; do\n"
        "  [[ -z \"$script_name\" || \"$script_name\" == \\#* ]] && continue\n"
        "  script_path=\"$STAGED_SCRIPTS/$script_name\"\n"
        "  if [ ! -r \"$script_path\" ]; then\n"
        "    echo \"> missing script $script_name\" >&2\n"
        "    exit 1\n"
        "  fi\n"
        "  bash --noprofile --norc -x \"$script_path\"\n"
        "  echo \"__SCRIPT_DONE__:$script_name\"\n"
        "done < \"$STAGED_SCRIPTS/install.sh\"\n";
    if (!writeFile(driverScriptPath, driverScript, true)) {
        return false;
    }

    if (geteuid() == 0) {
        if (!directory.mkpath(targetScriptsDirectory)) {
            if (errorMessage) {
                *errorMessage = QString("Unable to create `%1`.").arg(targetScriptsDirectory);
            }
            return false;
        }
        if (!directory.mkpath(targetFilesDirectory)) {
            if (errorMessage) {
                *errorMessage = QString("Unable to create `%1`.").arg(targetFilesDirectory);
            }
            return false;
        }

        if (!writeFile(QDir(targetScriptsDirectory).filePath("final_setup.sh"), buildFinalSetupScript(), true)) {
            return false;
        }
        if (!writeFile(QDir(targetScriptsDirectory).filePath("partition.sh"), buildPartitionScript(), true)) {
            return false;
        }
        if (!writeFile(QDir(targetScriptsDirectory).filePath("mount.sh"), buildMountScript(), true)) {
            return false;
        }
        if (!writeFile(QDir(targetFilesDirectory).filePath("hostname"), buildHostnameFile(), false)) {
            return false;
        }
        if (!writeFile(QDir(targetFilesDirectory).filePath("clock"), buildClockFile(), false)) {
            return false;
        }
        if (!writeFile(QDir(targetFilesDirectory).filePath("fstab"), buildFstabFile(), false)) {
            return false;
        }
    }

    if (runtimeScriptsDirectory) {
        *runtimeScriptsDirectory = geteuid() == 0 ? targetScriptsDirectory : stagedScriptsDirectory;
    }

    return true;
}

QStringList InstallerWindow::collectScriptPaths(const QString &scriptsDirectory) const
{
    const QString installListPath = QDir(scriptsDirectory).filePath("install.sh");
    QFile installListFile(installListPath);
    if (!installListFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    QStringList scriptPaths;
    while (!installListFile.atEnd()) {
        QString line = QString::fromUtf8(installListFile.readLine()).trimmed();
        if (line.isEmpty() || line.startsWith('#')) {
            continue;
        }

        if ((line.startsWith('"') && line.endsWith('"')) || (line.startsWith('\'') && line.endsWith('\''))) {
            line = line.mid(1, line.size() - 2);
        }

        if (!line.endsWith(".sh")) {
            continue;
        }

        const QString scriptPath = QDir(scriptsDirectory).filePath(line);
        if (QFileInfo(scriptPath).isFile()) {
            scriptPaths.append(QFileInfo(scriptPath).absoluteFilePath());
        }
    }

    return scriptPaths;
}

int InstallerWindow::countScriptSteps(const QStringList &scriptPaths) const
{
    const QRegularExpression stepPattern(R"(^\s*echo\s+['"]?step:.*?['"]?\s*$)", QRegularExpression::MultilineOption);
    int stepCount = 0;

    for (const QString &scriptPath : scriptPaths) {
        QFile scriptFile(scriptPath);
        if (!scriptFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }

        const QString scriptContents = QString::fromUtf8(scriptFile.readAll());
        auto matchIterator = stepPattern.globalMatch(scriptContents);
        while (matchIterator.hasNext()) {
            matchIterator.next();
            ++stepCount;
        }
    }

    return stepCount;
}

void InstallerWindow::startNextInstallScript()
{
    if (!installProcess_) {
        return;
    }

    if (currentInstallScriptIndex_ < 0 || currentInstallScriptIndex_ >= installScriptPaths_.size()) {
        installInProgress_ = false;
        installCompleted_ = true;
        currentInstallScriptPath_.clear();
        if (installProgressBar_) {
            if (totalInstallSteps_ > 0) {
                installProgressBar_->setValue(totalInstallSteps_);
            } else {
                installProgressBar_->setRange(0, 1);
                installProgressBar_->setValue(1);
            }
        }
        setInstallStatus("Current Step: Complete", QColor("#1b5e20"));
        appendInstallLogLine("> all scripts complete");
        updateNavigationState();
        return;
    }

    const QString driverScriptPath = QDir(currentRunDirectory_).filePath("generated-artifacts/install-driver.sh");
    const bool useInstallDriver = geteuid() != 0 && currentInstallScriptIndex_ == 0 && QFileInfo(driverScriptPath).isFile();
    const QString scriptPath = useInstallDriver ? driverScriptPath : installScriptPaths_.at(currentInstallScriptIndex_);
    const QFileInfo scriptInfo(scriptPath);
    if (!scriptInfo.exists() || !scriptInfo.isFile() || !scriptInfo.isReadable()) {
        installInProgress_ = false;
        installCompleted_ = false;
        appendInstallLogLine(QString("> script unavailable: %1").arg(scriptPath));
        setInstallStatus("Current Step: Failed", QColor("#b71c1c"));
        updateNavigationState();
        return;
    }

    const QString bashExecutable = QStandardPaths::findExecutable("bash");
    if (bashExecutable.isEmpty()) {
        installInProgress_ = false;
        installCompleted_ = false;
        appendInstallLogLine("> unable to find `bash` in PATH");
        setInstallStatus("Current Step: Failed", QColor("#b71c1c"));
        updateNavigationState();
        return;
    }

    currentInstallScriptPath_ = scriptPath;
    installOutputBuffer_.clear();
    pendingInstallStepText_.clear();
    installProcess_->setWorkingDirectory(scriptInfo.absolutePath());
    QProcessEnvironment processEnvironment = QProcessEnvironment::systemEnvironment();
    processEnvironment.remove("BASH_ENV");
    processEnvironment.remove("ENV");
    processEnvironment.insert("INSTALL_RUN_DIR", currentRunDirectory_);
    installProcess_->setProcessEnvironment(processEnvironment);

    QString program = bashExecutable;
    QStringList arguments = {"--noprofile", "--norc", "-x", scriptPath};
    QString commandDisplay = QString("$ %1 --noprofile --norc -x \"%2\"").arg(bashExecutable, scriptPath);

    if (useInstallDriver) {
        const QString sudoExecutable = QStandardPaths::findExecutable("sudo");
        const QString envExecutable = QStandardPaths::findExecutable("env");
        if (sudoExecutable.isEmpty() || envExecutable.isEmpty()) {
            installInProgress_ = false;
            installCompleted_ = false;
            appendInstallLogLine("> root access requires `sudo` and `env` in PATH");
            setInstallStatus("Current Step: Failed", QColor("#b71c1c"));
            updateNavigationState();
            return;
        }

        program = sudoExecutable;
        arguments = {
            "-n",
            envExecutable,
            QString("INSTALL_RUN_DIR=%1").arg(currentRunDirectory_),
            "BASH_ENV=",
            "ENV=",
            bashExecutable,
            "--noprofile",
            "--norc",
            "-x",
            scriptPath
        };
        commandDisplay = QString("$ %1 -n %2 INSTALL_RUN_DIR=%3 BASH_ENV= ENV= %4 --noprofile --norc -x \"%5\"")
                             .arg(sudoExecutable,
                                  envExecutable,
                                  shellQuote(currentRunDirectory_),
                                  bashExecutable,
                                  scriptPath);
    }

    appendInstallLogLine(commandDisplay);
    installProcess_->start(program, arguments);
}

void InstallerWindow::handleInstallProcessOutput()
{
    if (!installProcess_) {
        return;
    }

    installOutputBuffer_ += QString::fromLocal8Bit(installProcess_->readAll());
    int newlineIndex = installOutputBuffer_.indexOf('\n');
    while (newlineIndex >= 0) {
        QString line = installOutputBuffer_.left(newlineIndex);
        installOutputBuffer_.remove(0, newlineIndex + 1);
        if (line.endsWith('\r')) {
            line.chop(1);
        }
        processInstallOutputLine(line);
        newlineIndex = installOutputBuffer_.indexOf('\n');
    }
}

void InstallerWindow::handleInstallProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (!installOutputBuffer_.isEmpty()) {
        processInstallOutputLine(installOutputBuffer_);
        installOutputBuffer_.clear();
    }

    if (!installInProgress_) {
        return;
    }

    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        installInProgress_ = false;
        installCompleted_ = false;
        const QString scriptName = currentInstallScriptPath_.isEmpty()
                                       ? QStringLiteral("<unknown>")
                                       : QFileInfo(currentInstallScriptPath_).fileName();
        const QString statusText = exitStatus == QProcess::NormalExit ? QStringLiteral("normal-exit")
                                                                      : QStringLiteral("crashed");
        appendInstallLogLine(QString("> %1 failed with exit code %2 (%3)").arg(scriptName).arg(exitCode).arg(statusText));
        setInstallStatus("Current Step: Failed", QColor("#b71c1c"));
        updateNavigationState();
        return;
    }

    const QString driverScriptPath = QDir(currentRunDirectory_).filePath("generated-artifacts/install-driver.sh");
    if (QFileInfo(currentInstallScriptPath_).absoluteFilePath() == QFileInfo(driverScriptPath).absoluteFilePath()) {
        currentInstallScriptPath_.clear();
        currentInstallScriptIndex_ = installScriptPaths_.size();
        startNextInstallScript();
        return;
    }

    if (installProgressBar_ && totalInstallSteps_ > 0) {
        ++completedInstallSteps_;
        installProgressBar_->setValue(qMin(completedInstallSteps_, totalInstallSteps_));
    }

    currentInstallScriptPath_.clear();
    ++currentInstallScriptIndex_;
    startNextInstallScript();
}

void InstallerWindow::handleInstallProcessError(QProcess::ProcessError error)
{
    if (!installInProgress_ || !installProcess_) {
        return;
    }

    const QString scriptName = currentInstallScriptPath_.isEmpty()
                                   ? QStringLiteral("<unknown>")
                                   : QFileInfo(currentInstallScriptPath_).fileName();

    if (error == QProcess::FailedToStart || error == QProcess::Crashed) {
        installInProgress_ = false;
        installCompleted_ = false;
        appendInstallLogLine(QString("> %1 process error: %2").arg(scriptName, installProcess_->errorString()));
        setInstallStatus("Current Step: Failed", QColor("#b71c1c"));
        updateNavigationState();
        return;
    }

    appendInstallLogLine(QString("> %1 process warning: %2").arg(scriptName, installProcess_->errorString()));
}

void InstallerWindow::appendInstallLogLine(const QString &line)
{
    if (!installLog_) {
        return;
    }

    installLog_->appendPlainText(line);
    installLog_->centerCursor();
}

void InstallerWindow::processInstallOutputLine(const QString &line)
{
    if (line.isEmpty()) {
        return;
    }

    if (line.startsWith("__SCRIPT_DONE__:")) {
        const QString scriptName = line.mid(QString("__SCRIPT_DONE__:").size()).trimmed();
        appendInstallLogLine(QString("> completed %1").arg(scriptName));
        if (installProgressBar_ && totalInstallSteps_ > 0) {
            ++completedInstallSteps_;
            installProgressBar_->setValue(qMin(completedInstallSteps_, totalInstallSteps_));
        }
        return;
    }

    appendInstallLogLine(line);

    static const QRegularExpression tracedStepPattern(R"(^\+\s+echo\s+['"]?step:(.*?)['"]?\s*$)");

    QString stepText;
    bool tracedStep = false;
    if (line.startsWith("step:")) {
        stepText = line.mid(5).trimmed();
    } else {
        const QRegularExpressionMatch match = tracedStepPattern.match(line);
        if (match.hasMatch()) {
            tracedStep = true;
            stepText = match.captured(1).trimmed();
        }
    }

    if (stepText.isEmpty()) {
        return;
    }

    if (!tracedStep && pendingInstallStepText_ == stepText) {
        pendingInstallStepText_.clear();
        return;
    }

    pendingInstallStepText_ = tracedStep ? stepText : QString();
    setInstallStatus(QString("Current Step: %1").arg(stepText), QColor("#1b5e20"));
}

QString InstallerWindow::buildFinalSetupScript() const
{
    const QString username = usernameEdit_->text().trimmed();
    const QString password = passwordEdit_->text();

    QStringList lines;
    lines << "#!/usr/bin/env bash";
    lines << "";
    lines << "set -euo pipefail";
    lines << "";
    lines << "SCRIPT_DIR=\"$(cd -- \"$(dirname -- \"$0\")\" && pwd)\"";
    lines << "FILES_DIR=\"$SCRIPT_DIR/../files\"";
    lines << "";
    lines << "echo \"step:Finalizing setup\"";
    lines << QString("useradd -m %1").arg(shellQuote(username));
    lines << QString("printf '%s\\n' %1 | chpasswd").arg(shellQuote(username + ":" + password));
    lines << "install -Dm644 \"$FILES_DIR/hostname\" /etc/hostname";
    lines << "install -Dm644 \"$FILES_DIR/clock\" /etc/sysconfig/clock";
    lines << "install -Dm644 \"$FILES_DIR/fstab\" /etc/fstab";

    return lines.join('\n') + '\n';
}

QString InstallerWindow::buildPartitionScript() const
{
    const DriveInfo drive = currentDrive();
    const QVector<PlannedPartition> partitions = collectPartitions();

    QStringList lines;
    QStringList formatLines;
    lines << "#!/usr/bin/env bash";
    lines << "";
    lines << "set -euo pipefail";
    lines << "";
    lines << QString("TARGET_DRIVE=%1").arg(shellQuote(drive.path));
    lines << "";
    lines << "echo \"step:Setting up partitions\"";
    lines << "parted --script \"$TARGET_DRIVE\" mklabel gpt";

    qint64 startMiB = 1;
    for (int row = 0; row < partitions.size(); ++row) {
        const PlannedPartition &partition = partitions.at(row);
        const qint64 sizeMiB = qMax<qint64>(1, static_cast<qint64>((partition.sizeGiB * 1024.0) + 0.5));
        const qint64 endMiB = startMiB + sizeMiB;
        const QString devicePath = partitionNodeName(drive.path, row);

        lines << QString("parted --script \"$TARGET_DRIVE\" mkpart primary %1 %2MiB %3MiB")
                     .arg(partedFileSystem(partition))
                     .arg(startMiB)
                     .arg(endMiB);
        if (partition.mountPoint == "/boot/efi") {
            lines << QString("parted --script \"$TARGET_DRIVE\" set %1 esp on").arg(row + 1);
            lines << QString("parted --script \"$TARGET_DRIVE\" set %1 boot on").arg(row + 1);
        }

        formatLines << QString("# %1 -> %2 (%3, %4 GiB)")
                            .arg(devicePath,
                                 partition.mountPoint,
                                 partition.fileSystem,
                                 QString::number(partition.sizeGiB, 'f', 1));
        formatLines << mkfsCommand(partition, devicePath);
        startMiB = endMiB;
    }

    lines << "partprobe \"$TARGET_DRIVE\"";
    lines << "";
    lines << formatLines;

    return lines.join('\n') + '\n';
}

QString InstallerWindow::buildMountScript() const
{
    const DriveInfo drive = currentDrive();
    const QVector<PlannedPartition> partitions = collectPartitions();

    QStringList lines;
    lines << "#!/usr/bin/env bash";
    lines << "";
    lines << "set -euo pipefail";
    lines << "";
    lines << "echo \"step:Mounting filesystems\"";

    const auto appendMountCommandsFor = [&lines, &drive, &partitions](const QString &mountPoint) {
        for (int row = 0; row < partitions.size(); ++row) {
            const PlannedPartition &partition = partitions.at(row);
            if (partition.mountPoint != mountPoint) {
                continue;
            }

            const QString devicePath = partitionNodeName(drive.path, row);
            if (partition.mountPoint == "swap") {
                lines << QString("swapon %1").arg(shellQuote(devicePath));
                continue;
            }

            lines << QString("mkdir -p %1").arg(shellQuote(partition.localMountPoint));
            lines << QString("mount %1 %2").arg(shellQuote(devicePath), shellQuote(partition.localMountPoint));
        }
    };

    appendMountCommandsFor("/");
    appendMountCommandsFor("/boot");
    appendMountCommandsFor("/boot/efi");
    appendMountCommandsFor("/home");
    appendMountCommandsFor("/var");

    for (int row = 0; row < partitions.size(); ++row) {
        const PlannedPartition &partition = partitions.at(row);
        if (partition.mountPoint == "/" ||
            partition.mountPoint == "/boot" ||
            partition.mountPoint == "/boot/efi" ||
            partition.mountPoint == "/home" ||
            partition.mountPoint == "/var" ||
            partition.mountPoint == "swap") {
            continue;
        }

        const QString devicePath = partitionNodeName(drive.path, row);
        lines << QString("mkdir -p %1").arg(shellQuote(partition.localMountPoint));
        lines << QString("mount %1 %2").arg(shellQuote(devicePath), shellQuote(partition.localMountPoint));
    }

    appendMountCommandsFor("swap");

    return lines.join('\n') + '\n';
}

QString InstallerWindow::buildHostnameFile() const
{
    return hostnameEdit_->text().trimmed() + '\n';
}

QString InstallerWindow::buildClockFile() const
{
    return QString("ZONE=\"%1\"\n").arg(timeZoneCombo_->currentText().trimmed());
}

QString InstallerWindow::buildFstabFile() const
{
    const DriveInfo drive = currentDrive();
    const QVector<PlannedPartition> partitions = collectPartitions();

    QStringList lines;
    lines << "# <file system> <mount point> <type> <options> <dump> <pass>";
    for (int row = 0; row < partitions.size(); ++row) {
        const PlannedPartition &partition = partitions.at(row);
        const QString devicePath = partitionNodeName(drive.path, row);
        if (partition.mountPoint == "swap") {
            lines << QString("%1\tswap\tswap\tdefaults\t0\t0").arg(devicePath);
            continue;
        }

        lines << QString("%1\t%2\t%3\tdefaults\t0\t%4")
                     .arg(devicePath,
                          partition.mountPoint,
                          fstabFileSystem(partition))
                     .arg(fstabPassNumber(partition));
    }

    return lines.join('\n') + '\n';
}

QString InstallerWindow::buildConfigText() const
{
    QStringList lines;
    lines << "LFS Installer Setup Summary";
    lines << "";
    lines << "System";
    lines << QString("Hostname: %1").arg(hostnameEdit_->text().trimmed());
    lines << QString("Username: %1").arg(usernameEdit_->text().trimmed());
    lines << QString("Time zone: %1").arg(timeZoneCombo_->currentText());
    lines << "";

    const DriveInfo drive = currentDrive();
    lines << "Storage";
    lines << QString("Target drive: %1").arg(drive.path.isEmpty() ? QStringLiteral("(not selected)") : driveLabel(drive));

    const QVector<PlannedPartition> partitions = collectPartitions();
    if (partitions.isEmpty()) {
        lines << "Partitions: none";
    } else {
        lines << "Partitions:";
        for (const PlannedPartition &partition : partitions) {
            lines << QString("  - mount=%1 local=%2 fs=%3 size=%4 GiB format=%5")
                         .arg(partition.mountPoint,
                              partition.localMountPoint,
                              partition.fileSystem,
                              QString::number(partition.sizeGiB, 'f', 1),
                              partition.format ? "yes" : "no");
        }
    }

    lines << "";
    lines << "Install";
    lines << QString("Script source: %1").arg(repoUrlEdit_->text().trimmed());
    lines << QString("Branch: %1").arg(repoBranchEdit_->text().trimmed());
    lines << QString("Script path: %1").arg(scriptPathEdit_->text().trimmed());
    lines << QString("Working root: %1").arg(workRootEdit_->text().trimmed());
    lines << QString("Run directory: %1").arg(currentRunDirectory_);

    const QStringList features = collectSelectedFeatures();
    lines << "";
    lines << "Features";
    if (features.isEmpty()) {
        lines << "None selected yet";
    } else {
        for (const QString &feature : features) {
            lines << QString("  - %1").arg(feature);
        }
    }

    lines << "";
    lines << "Generated by GUI-only install checkpoint.";
    lines << "No real install command has been executed yet.";

    return lines.join('\n') + '\n';
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
