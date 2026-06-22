#include "installerwindow.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFontDatabase>
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
#include <QProcess>
#include <QProcessEnvironment>
#include <QProgressBar>
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

QString comboStyleForFileSystem(const QString &fileSystem)
{
    const QColor color = fileSystemColor(fileSystem).lighter(165);
    return QString("QComboBox { background: %1; border: 1px solid #b9bfc7; padding: 2px 6px; }")
        .arg(color.name());
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
    page->setObjectName("partitionPage");
    page->setStyleSheet(R"(
        QWidget#partitionPage {
            background: #d8d8d8;
        }
        QFrame#partitionToolbar,
        QFrame#partitionShell,
        QFrame#partitionMapFrame,
        QFrame#partitionStatusFrame,
        QGroupBox#partitionDeviceBox {
            background: #f5f5f5;
            border: 3px solid #111111;
        }
        QLabel#sectionTitle {
            font-size: 18px;
            font-weight: 700;
            color: #1f1f1f;
        }
        QLabel#driveSummary,
        QLabel#partitionStatus {
            color: #2f2f2f;
        }
        QGroupBox#partitionDeviceBox::title {
            subcontrol-origin: margin;
            left: 12px;
            padding: 0 6px;
            color: #1f1f1f;
            font-weight: 600;
        }
        QPushButton#partitionButton {
            min-width: 96px;
            min-height: 30px;
            padding: 4px 10px;
            background: #ffffff;
            border: 3px solid #111111;
            color: #111111;
        }
        QPushButton#partitionButton:disabled {
            color: #808080;
            border-color: #808080;
        }
        QHeaderView::section {
            background: #dcdcdc;
            color: #111111;
            border: 2px solid #111111;
            padding: 4px 6px;
            font-weight: 600;
        }
        QTableWidget,
        QTreeWidget {
            background: #ffffff;
            alternate-background-color: #f0f0f0;
            border: 3px solid #111111;
            gridline-color: #111111;
            color: #111111;
        }
        QComboBox,
        QDoubleSpinBox {
            border: 2px solid #111111;
            padding: 2px 6px;
            background: #ffffff;
        }
    )");
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(10);

    auto *toolbar = new QFrame(page);
    toolbar->setObjectName("partitionToolbar");
    auto *toolLayout = new QHBoxLayout(toolbar);
    toolLayout->setContentsMargins(10, 10, 10, 10);
    toolLayout->setSpacing(8);

    toolLayout->addWidget(new QLabel("Device", toolbar));
    driveCombo_ = new QComboBox(toolbar);
    driveCombo_->setMinimumWidth(260);
    toolLayout->addWidget(driveCombo_);

    auto *refreshButton = new QPushButton("Refresh", toolbar);
    auto *addButton = new QPushButton("Add", toolbar);
    auto *removeButton = new QPushButton("Delete", toolbar);
    auto *autoButton = new QPushButton("Auto Layout", toolbar);
    const QList<QPushButton *> toolButtons = {refreshButton, addButton, removeButton, autoButton};
    for (QPushButton *button : toolButtons) {
        button->setObjectName("partitionButton");
        toolLayout->addWidget(button);
    }
    toolLayout->addStretch(1);
    layout->addWidget(toolbar);

    auto *editorShell = new QFrame(page);
    editorShell->setObjectName("partitionShell");
    auto *editorLayout = new QVBoxLayout(editorShell);
    editorLayout->setContentsMargins(10, 10, 10, 10);
    editorLayout->setSpacing(8);

    auto *header = new QLabel("Page 2: Partition Layout", editorShell);
    header->setObjectName("sectionTitle");
    editorLayout->addWidget(header);

    driveDetailsLabel_ = new QLabel(editorShell);
    driveDetailsLabel_->setObjectName("driveSummary");
    driveDetailsLabel_->setWordWrap(true);
    editorLayout->addWidget(driveDetailsLabel_);

    auto *mapFrame = new QFrame(editorShell);
    mapFrame->setObjectName("partitionMapFrame");
    auto *mapLayout = new QVBoxLayout(mapFrame);
    mapLayout->setContentsMargins(10, 10, 10, 10);
    mapLayout->setSpacing(8);

    auto *mapHeaderLayout = new QHBoxLayout();
    mapHeaderLayout->addWidget(new QLabel("Partition Map", mapFrame));
    mapHeaderLayout->addStretch(1);
    partitionCapacityLabel_ = new QLabel(mapFrame);
    partitionCapacityLabel_->setObjectName("partitionStatus");
    mapHeaderLayout->addWidget(partitionCapacityLabel_);
    mapLayout->addLayout(mapHeaderLayout);

    partitionMapWidget_ = new QWidget(mapFrame);
    auto *partitionMapLayout = new QHBoxLayout(partitionMapWidget_);
    partitionMapLayout->setContentsMargins(0, 0, 0, 0);
    partitionMapLayout->setSpacing(6);
    mapLayout->addWidget(partitionMapWidget_);

    partitionTable_ = new QTableWidget(0, 8, editorShell);
    partitionTable_->setHorizontalHeaderLabels({"Partition", "File System", "Mount Point", "Label", "Size", "Used", "Unused", "Flags"});
    partitionTable_->setAlternatingRowColors(true);
    partitionTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    partitionTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    partitionTable_->setShowGrid(true);
    partitionTable_->verticalHeader()->setVisible(false);
    partitionTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    partitionTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    partitionTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    partitionTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    partitionTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    partitionTable_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    partitionTable_->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    partitionTable_->horizontalHeader()->setSectionResizeMode(7, QHeaderView::ResizeToContents);
    partitionTable_->verticalHeader()->setDefaultSectionSize(34);

    editorLayout->addWidget(mapFrame);
    editorLayout->addWidget(partitionTable_, 1);

    auto *statusFrame = new QFrame(editorShell);
    statusFrame->setObjectName("partitionStatusFrame");
    auto *statusLayout = new QHBoxLayout(statusFrame);
    statusLayout->setContentsMargins(10, 6, 10, 6);
    partitionOperationsLabel_ = new QLabel("0 operations pending", statusFrame);
    partitionOperationsLabel_->setObjectName("partitionStatus");
    statusLayout->addWidget(partitionOperationsLabel_);
    statusLayout->addStretch(1);
    editorLayout->addWidget(statusFrame);

    layout->addWidget(editorShell, 1);

    auto *deviceBox = new QGroupBox("Detected Devices", page);
    deviceBox->setObjectName("partitionDeviceBox");
    auto *deviceLayout = new QVBoxLayout(deviceBox);
    deviceLayout->setContentsMargins(10, 14, 10, 10);
    deviceLayout->setSpacing(8);
    deviceTree_ = new QTreeWidget(deviceBox);
    deviceTree_->setColumnCount(2);
    deviceTree_->setHeaderLabels({"Device", "Details"});
    deviceTree_->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    deviceTree_->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    deviceTree_->setAlternatingRowColors(true);
    deviceLayout->addWidget(deviceTree_);
    deviceBox->setMaximumHeight(220);
    layout->addWidget(deviceBox);

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
    connect(partitionTable_, &QTableWidget::itemSelectionChanged, this, &InstallerWindow::refreshPartitionEditorPreview);

    addPartitionRow("/", "ext4", 40.0, true);
    addPartitionRow("swap", "swap", 4.0, true);

    return page;
}

QWidget *InstallerWindow::buildInstallPage()
{
    auto *page = new QWidget(this);
    page->setObjectName("stagePage");
    page->setStyleSheet(R"(
        QWidget#stagePage {
            background: #e2e2e2;
        }
        QFrame#stageFrame {
            background: #f7f7f7;
            border: 3px solid #111111;
        }
        QLabel#stageHeader {
            color: #101010;
            font-family: monospace;
            font-size: 15px;
            font-weight: 600;
        }
        QLabel#stageSection {
            color: #101010;
            font-family: monospace;
            font-size: 13px;
        }
        QFrame#progressFrame,
        QFrame#terminalFrame {
            background: #ffffff;
            border: 3px solid #111111;
        }
        QProgressBar#stageProgress {
            background: #ffffff;
            border: 2px solid #111111;
            text-align: center;
            font-weight: 600;
            min-height: 28px;
        }
        QProgressBar#stageProgress::chunk {
            background: #2f6fed;
        }
        QTextEdit#stageLog {
            background: #0f1115;
            color: #d7e3ff;
            border: none;
            selection-background-color: #2f6bff;
        }
        QPushButton#stageButton {
            min-width: 110px;
            min-height: 36px;
            background: #ffffff;
            border: 3px solid #111111;
            color: #111111;
            font-family: monospace;
            font-size: 13px;
            font-weight: 600;
        }
        QPushButton#stageButton:disabled {
            color: #7c7c7c;
            border-color: #7c7c7c;
        }
    )");

    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 22, 28, 22);
    layout->setSpacing(0);

    auto *stageFrame = new QFrame(page);
    stageFrame->setObjectName("stageFrame");
    stageFrame->setMaximumWidth(760);
    auto *stageLayout = new QVBoxLayout(stageFrame);
    stageLayout->setContentsMargins(16, 16, 16, 16);
    stageLayout->setSpacing(12);

    installStatusLabel_ = new QLabel("Current Step: Ready to start installation", stageFrame);
    installStatusLabel_->setObjectName("stageHeader");
    installStatusLabel_->setWordWrap(true);
    stageLayout->addWidget(installStatusLabel_);

    auto *progressFrame = new QFrame(stageFrame);
    progressFrame->setObjectName("progressFrame");
    auto *progressLayout = new QVBoxLayout(progressFrame);
    progressLayout->setContentsMargins(10, 10, 10, 10);
    progressLayout->setSpacing(6);

    auto *progressLabel = new QLabel("Install Progress", progressFrame);
    progressLabel->setObjectName("stageSection");
    progressLayout->addWidget(progressLabel);

    installProgressBar_ = new QProgressBar(progressFrame);
    installProgressBar_->setObjectName("stageProgress");
    installProgressBar_->setRange(0, 100);
    installProgressBar_->setValue(0);
    installProgressBar_->setFormat("%p%");
    installProgressBar_->setTextVisible(true);
    progressLayout->addWidget(installProgressBar_);
    stageLayout->addWidget(progressFrame);

    auto *terminalFrame = new QFrame(stageFrame);
    terminalFrame->setObjectName("terminalFrame");
    auto *terminalLayout = new QVBoxLayout(terminalFrame);
    terminalLayout->setContentsMargins(10, 10, 10, 10);
    terminalLayout->setSpacing(6);

    auto *logLabel = new QLabel("Install Output", terminalFrame);
    logLabel->setObjectName("stageSection");
    terminalLayout->addWidget(logLabel);

    installLog_ = new QTextEdit(terminalFrame);
    installLog_->setObjectName("stageLog");
    installLog_->setReadOnly(true);
    installLog_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    installLog_->setLineWrapMode(QTextEdit::NoWrap);
    installLog_->setMinimumHeight(320);
    installLog_->setPlainText("$ installer idle\n$ output will appear here when the install hook is wired up");
    terminalLayout->addWidget(installLog_, 1);
    stageLayout->addWidget(terminalFrame, 1);

    auto *buttonRow = new QHBoxLayout();
    buttonRow->setContentsMargins(0, 0, 0, 0);
    pageThreeBackButton_ = new QPushButton("Previous", stageFrame);
    pageThreeBackButton_->setObjectName("stageButton");
    pageThreeInstallButton_ = new QPushButton("Install", stageFrame);
    pageThreeInstallButton_->setObjectName("stageButton");
    buttonRow->addWidget(pageThreeBackButton_, 0, Qt::AlignLeft);
    buttonRow->addStretch(1);
    buttonRow->addWidget(pageThreeInstallButton_, 0, Qt::AlignRight);
    stageLayout->addLayout(buttonRow);

    layout->addWidget(stageFrame, 0, Qt::AlignTop | Qt::AlignLeft);
    layout->addStretch(1);

    repoUrlEdit_ = new QLineEdit(page);
    repoUrlEdit_->setText("local-install-script");
    repoBranchEdit_ = new QLineEdit(page);
    repoBranchEdit_->setText("main");
    scriptPathEdit_ = new QLineEdit(page);
    scriptPathEdit_->setText("install.sh");
    workRootEdit_ = new QLineEdit(page);
    workRootEdit_->setText("/tmp/lfs-installer");

    connect(pageThreeBackButton_, &QPushButton::clicked, this, &InstallerWindow::handleBackAction);
    connect(pageThreeInstallButton_, &QPushButton::clicked, this, &InstallerWindow::handlePrimaryAction);

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

    auto *partitionItem = new QTableWidgetItem();
    partitionItem->setFlags(partitionItem->flags() & ~Qt::ItemIsEditable);
    partitionTable_->setItem(row, 0, partitionItem);

    auto *mountCombo = new QComboBox(partitionTable_);
    mountCombo->setEditable(true);
    mountCombo->addItems({"/boot/efi", "/boot", "/", "/home", "/var", "swap"});
    mountCombo->setCurrentText(mountPoint);
    connect(mountCombo, &QComboBox::currentTextChanged, this, &InstallerWindow::markInstallDirty);
    partitionTable_->setCellWidget(row, 2, mountCombo);

    auto *fsCombo = new QComboBox(partitionTable_);
    fsCombo->addItems({"ext4", "xfs", "btrfs", "fat32", "swap"});
    const int fsIndex = fsCombo->findText(fileSystem);
    fsCombo->setCurrentIndex(fsIndex >= 0 ? fsIndex : 0);
    fsCombo->setStyleSheet(comboStyleForFileSystem(fsCombo->currentText()));
    connect(fsCombo, &QComboBox::currentTextChanged, this, [this, fsCombo](const QString &) {
        fsCombo->setStyleSheet(comboStyleForFileSystem(fsCombo->currentText()));
        markInstallDirty();
    });
    partitionTable_->setCellWidget(row, 1, fsCombo);

    auto *labelItem = new QTableWidgetItem();
    labelItem->setFlags(labelItem->flags() & ~Qt::ItemIsEditable);
    partitionTable_->setItem(row, 3, labelItem);

    auto *sizeSpin = new QDoubleSpinBox(partitionTable_);
    sizeSpin->setRange(0.1, 102400.0);
    sizeSpin->setDecimals(1);
    sizeSpin->setSuffix(" GiB");
    sizeSpin->setValue(sizeGiB);
    connect(sizeSpin, &QDoubleSpinBox::valueChanged, this, [this](double) {
        markInstallDirty();
    });
    partitionTable_->setCellWidget(row, 4, sizeSpin);

    auto *usedItem = new QTableWidgetItem();
    usedItem->setFlags(usedItem->flags() & ~Qt::ItemIsEditable);
    partitionTable_->setItem(row, 5, usedItem);

    auto *unusedItem = new QTableWidgetItem();
    unusedItem->setFlags(unusedItem->flags() & ~Qt::ItemIsEditable);
    partitionTable_->setItem(row, 6, unusedItem);

    auto *formatCheck = new QCheckBox(partitionTable_);
    formatCheck->setChecked(format);
    formatCheck->setText(flagsText({mountPoint, fileSystem, sizeGiB, format}));
    connect(formatCheck, &QCheckBox::checkStateChanged, this, &InstallerWindow::markInstallDirty);
    partitionTable_->setCellWidget(row, 7, formatCheck);
}

QVector<PlannedPartition> InstallerWindow::collectPartitions() const
{
    QVector<PlannedPartition> partitions;
    for (int row = 0; row < partitionTable_->rowCount(); ++row) {
        auto *mountCombo = qobject_cast<QComboBox *>(partitionTable_->cellWidget(row, 2));
        auto *fsCombo = qobject_cast<QComboBox *>(partitionTable_->cellWidget(row, 1));
        auto *sizeSpin = qobject_cast<QDoubleSpinBox *>(partitionTable_->cellWidget(row, 4));
        auto *formatCheck = qobject_cast<QCheckBox *>(partitionTable_->cellWidget(row, 7));

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

    for (int row = 0; row < partitions.size(); ++row) {
        if (auto *partitionItem = partitionTable_->item(row, 0)) {
            partitionItem->setText(partitionNodeName(drive.path, row));
        }
        if (auto *labelItem = partitionTable_->item(row, 3)) {
            labelItem->setText(partitionLabelText(partitions.at(row).mountPoint));
        }
        if (auto *usedItem = partitionTable_->item(row, 5)) {
            usedItem->setText(usedText(partitions.at(row)));
        }
        if (auto *unusedItem = partitionTable_->item(row, 6)) {
            unusedItem->setText(unusedText(partitions.at(row)));
        }
        if (auto *formatCheck = qobject_cast<QCheckBox *>(partitionTable_->cellWidget(row, 7))) {
            formatCheck->setText(flagsText(partitions.at(row)));
        }
    }

    if (drive.path.isEmpty()) {
        partitionCapacityLabel_->setText(QString("Planned %1 GiB").arg(QString::number(plannedGiB, 'f', 2)));
    } else {
        partitionCapacityLabel_->setText(
            QString("Planned %1 of %2 GiB   Unallocated %3 GiB")
                .arg(QString::number(plannedGiB, 'f', 2),
                     QString::number(driveGiB, 'f', 2),
                     QString::number(unallocatedGiB, 'f', 2)));
    }

    QStringList operations;
    for (int row = 0; row < partitions.size(); ++row) {
        operations.append(QString("%1 %2 (%3)")
                              .arg(partitionNodeName(drive.path, row),
                                   partitions.at(row).mountPoint,
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
        emptyLabel->setStyleSheet("color: #444444; background: #ffffff; border: 3px solid #111111; padding: 18px;");
        mapLayout->addWidget(emptyLabel);
        return;
    }

    const int selectedRow = partitionTable_->currentRow();
    for (int row = 0; row < partitions.size(); ++row) {
        const PlannedPartition &partition = partitions.at(row);
        const QColor color = fileSystemColor(partition.fileSystem);
        const QString borderColor = row == selectedRow ? QStringLiteral("#245dcb") : QStringLiteral("#111111");

        auto *segment = new QFrame(partitionMapWidget_);
        segment->setMinimumWidth(88);
        segment->setStyleSheet(
            QString("QFrame { background: %1; border: 3px solid %2; } QLabel { color: #111111; background: transparent; }")
                .arg(color.name(), borderColor));

        auto *segmentLayout = new QVBoxLayout(segment);
        segmentLayout->setContentsMargins(10, 10, 10, 10);
        segmentLayout->setSpacing(4);

        auto *topLabel = new QLabel(partitionNodeName(drive.path, row), segment);
        topLabel->setAlignment(Qt::AlignCenter);
        segmentLayout->addWidget(topLabel);

        auto *bottomLabel = new QLabel(QString("%1\n%2 GiB").arg(partitionLabelText(partition.mountPoint),
                                                                  QString::number(partition.sizeGiB, 'f', 2)),
                                       segment);
        bottomLabel->setAlignment(Qt::AlignCenter);
        bottomLabel->setStyleSheet("font-size: 16px; font-weight: 600;");
        segmentLayout->addWidget(bottomLabel, 1);

        int stretch = static_cast<int>(partition.sizeGiB * 10.0);
        if (stretch < 1) {
            stretch = 1;
        }
        mapLayout->addWidget(segment, stretch);
    }

    if (unallocatedGiB > 0.05) {
        auto *segment = new QFrame(partitionMapWidget_);
        segment->setMinimumWidth(88);
        segment->setStyleSheet("QFrame { background: #c2c2c2; border: 3px solid #111111; } QLabel { color: #111111; background: transparent; }");
        auto *segmentLayout = new QVBoxLayout(segment);
        segmentLayout->setContentsMargins(10, 10, 10, 10);
        segmentLayout->setSpacing(4);

        auto *label = new QLabel(QString("unallocated\n%1 GiB").arg(QString::number(unallocatedGiB, 'f', 2)), segment);
        label->setAlignment(Qt::AlignCenter);
        label->setStyleSheet("font-size: 16px; font-weight: 600;");
        segmentLayout->addWidget(label, 1);

        int stretch = static_cast<int>(unallocatedGiB * 10.0);
        if (stretch < 1) {
            stretch = 1;
        }
        mapLayout->addWidget(segment, stretch);
    }
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
        refreshPartitionEditorPreview();
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
        driveDetailsLabel_->setText("Select a target disk to update the device entry, partition map, and planner table.");
        refreshPartitionEditorPreview();
        return;
    }

    driveDetailsLabel_->setText(QString("Current device: %1").arg(driveLabel(drive)));
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
    if (installProgressBar_) {
        installProgressBar_->setValue(20);
    }
    installLog_->append("$ prepare run directory");
    installLog_->append(QString("> %1").arg(currentRunDirectory_));
    installLog_->append("$ write install-config.json");
    installCompleted_ = false;
    installCompleted_ = true;
    installLog_->append("> ok");
    installLog_->append("$ install command hook");
    installLog_->append("> not wired yet");
    if (installProgressBar_) {
        installProgressBar_->setValue(100);
    }
    setInstallStatus("Current Step: Config prepared. Waiting for install command wiring.", QColor("#1b5e20"));
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
    if (installProgressBar_) {
        installProgressBar_->setValue(0);
    }
    if (!reason.isEmpty()) {
        setInstallStatus(QString("Current Step: %1").arg(reason), QColor("#111111"));
    } else if (installStatusLabel_) {
        setInstallStatus("Current Step: Ready to start installation", QColor("#111111"));
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
