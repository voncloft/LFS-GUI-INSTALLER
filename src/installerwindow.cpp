#include "installerwindow.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QDomDocument>
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
#include <QScrollBar>
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
constexpr auto kMlfsBookSentinel = "@MLFS_BOOK@";
constexpr auto kMlfsBookRepositoryUrl = "https://github.com/lfs-book/lfs.git";
constexpr auto kMlfsBookBranch = "multilib";
constexpr auto kMlfsProfileRevision = "systemd";
constexpr auto kLfsPackagesArchiveName = "lfs-packages-13.0.tar";
constexpr auto kLfsPackagesArchiveUrl = "https://ftp.osuosl.org/pub/lfs/lfs-packages/lfs-packages-13.0.tar";

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

QString nodeLocalName(const QDomNode &node)
{
    if (!node.localName().isEmpty()) {
        return node.localName();
    }
    return node.nodeName();
}

QList<QDomElement> directChildElements(const QDomElement &parent, const QString &localName = {})
{
    QList<QDomElement> result;
    for (QDomNode child = parent.firstChild(); !child.isNull(); child = child.nextSibling()) {
        if (!child.isElement()) {
            continue;
        }

        const QDomElement element = child.toElement();
        if (!localName.isEmpty() && nodeLocalName(element) != localName) {
            continue;
        }
        result.append(element);
    }
    return result;
}

QString slugify(const QString &value)
{
    QString slug = value.toLower().trimmed();
    slug.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("-"));
    slug.remove(QRegularExpression(QStringLiteral("^-+|-+$")));
    return slug;
}

QString installShellHandoffHelpers()
{
    return QStringLiteral(R"SH(
__codex_handoff_lfs() {
  local marker="$1"
  printf '%s\n' "$marker"
  su - lfs
}

__codex_handoff_lfs_profile() {
  local marker="$1"
  printf '%s\n' "$marker"
  source ~/.bash_profile
}

__codex_handoff_chroot() {
  local marker="$1"
  printf '%s\n' "$marker"
  chroot "$LFS" /usr/bin/env -i \
      HOME=/root \
      TERM="$TERM" \
      PS1='(lfs chroot) \u:\w\$ ' \
      PATH=/usr/bin:/usr/sbin \
      MAKEFLAGS="-j$(nproc)" \
      TESTSUITEFLAGS="-j$(nproc)" \
      /bin/bash --login
}

__codex_handoff_login_bash() {
  local marker="$1"
  printf '%s\n' "$marker"
  exec /usr/bin/bash --login
}
)SH");
}

QString rewriteInteractiveHandoffs(QString scriptContents,
                                   const QString &entryName,
                                   bool *inlineDoneMarker)
{
    bool usesInlineDoneMarker = false;
    const QString doneMarker = shellQuote(QStringLiteral("__SCRIPT_DONE__:") + entryName);

    const QRegularExpression chrootPattern(
        QStringLiteral(R"CHROOT(chroot\s+"\$LFS"\s+/usr/bin/env\s+-i[\s\\\n]+HOME=/root[\s\\\n]+TERM="\$TERM"[\s\\\n]+PS1='\(lfs chroot\)\s+\\u:\\w\\\$ '[\s\\\n]+PATH=/usr/bin:/usr/sbin[\s\\\n]+MAKEFLAGS="-j\$\(nproc\)"[\s\\\n]+TESTSUITEFLAGS="-j\$\(nproc\)"[\s\\\n]+/bin/bash\s+--login)CHROOT"));
    if (scriptContents.contains(chrootPattern)) {
        scriptContents.replace(chrootPattern, QStringLiteral("__codex_handoff_chroot %1").arg(doneMarker));
        usesInlineDoneMarker = true;
    }

    QStringList rewrittenLines;
    const QStringList sourceLines = scriptContents.split(QLatin1Char('\n'));
    rewrittenLines.reserve(sourceLines.size());
    for (const QString &line : sourceLines) {
        const QString trimmed = line.trimmed();
        if (trimmed == QStringLiteral("su - lfs")) {
            rewrittenLines << QStringLiteral("__codex_handoff_lfs %1").arg(doneMarker);
            usesInlineDoneMarker = true;
            continue;
        }
        if (trimmed == QStringLiteral("source ~/.bash_profile")) {
            rewrittenLines << QStringLiteral("__codex_handoff_lfs_profile %1").arg(doneMarker);
            usesInlineDoneMarker = true;
            continue;
        }
        if (trimmed == QStringLiteral("exec /usr/bin/bash --login")
            || trimmed == QStringLiteral("exec /bin/bash --login")) {
            rewrittenLines << QStringLiteral("__codex_handoff_login_bash %1").arg(doneMarker);
            usesInlineDoneMarker = true;
            continue;
        }
        rewrittenLines << line;
    }

    if (inlineDoneMarker) {
        *inlineDoneMarker = usesInlineDoneMarker;
    }
    return rewrittenLines.join(QLatin1Char('\n'));
}

struct MlfsCommand
{
    QString text;
    bool noDump = false;
};

void collectMlfsCommands(const QDomNode &node, bool insideNoDump, QList<MlfsCommand> *commands)
{
    if (!commands) {
        return;
    }

    if (node.isElement()) {
        const QDomElement element = node.toElement();
        const bool noDump = insideNoDump || element.attribute(QStringLiteral("role")) == QStringLiteral("nodump");
        if (nodeLocalName(element) == QStringLiteral("userinput")) {
            const QDomNode parentNode = node.parentNode();
            if (parentNode.isElement() && nodeLocalName(parentNode.toElement()) == QStringLiteral("screen")) {
                const QString text = element.text().trimmed();
                if (!text.isEmpty()) {
                    commands->append({text, noDump});
                }
            }
            return;
        }

        for (QDomNode child = element.firstChild(); !child.isNull(); child = child.nextSibling()) {
            collectMlfsCommands(child, noDump, commands);
        }
        return;
    }

    for (QDomNode child = node.firstChild(); !child.isNull(); child = child.nextSibling()) {
        collectMlfsCommands(child, insideNoDump, commands);
    }
}

QString sectionTitle(const QDomElement &section)
{
    const QList<QDomElement> titles = directChildElements(section, QStringLiteral("title"));
    if (titles.isEmpty()) {
        return {};
    }
    return titles.constFirst().text().trimmed();
}

QString generatedMlfsScriptPath(int chapterNumber, int sectionNumber, const QString &slug)
{
    const QString chapterDirectory = QStringLiteral("mlfs-generated/chapter%1").arg(chapterNumber);
    const QString fileName = QStringLiteral("%1-%2.sh")
                                 .arg(sectionNumber, 3, 10, QChar('0'))
                                 .arg(slug);
    return QDir(chapterDirectory).filePath(fileName);
}

QString archiveBaseNameFromUrl(const QString &urlText)
{
    const QString fileName = QFileInfo(QUrl(urlText.trimmed()).path()).fileName();
    QString base = fileName;
    const QStringList suffixes = {
        QStringLiteral(".tar.xz"),
        QStringLiteral(".tar.gz"),
        QStringLiteral(".tar.bz2"),
        QStringLiteral(".tar.zst"),
        QStringLiteral(".tar.lz"),
        QStringLiteral(".tar.lz4")
    };
    for (const QString &suffix : suffixes) {
        if (base.endsWith(suffix)) {
            base.chop(suffix.size());
            break;
        }
    }
    return base;
}

QString generatedMlfsScriptBody(const QString &stepTitle,
                                const QStringList &commands,
                                const QString &setupCommands = QString(),
                                const QString &cleanupCommands = QString())
{
    QStringList lines;
    lines << QStringLiteral("#!/usr/bin/env bash");
    lines << QString();
    lines << QStringLiteral("echo %1").arg(shellQuote(QStringLiteral("step:") + stepTitle));
    lines << QStringLiteral("unset source_dir");
    if (!setupCommands.trimmed().isEmpty()) {
        lines << setupCommands.trimmed();
    }
    if (!commands.isEmpty()) {
        lines << commands;
    }
    if (!cleanupCommands.trimmed().isEmpty()) {
        lines << cleanupCommands.trimmed();
    }
    return lines.join('\n') + '\n';
}

bool writeGeneratedTextFile(const QString &path,
                            const QString &contents,
                            bool executable,
                            QString *errorMessage)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Unable to open `%1` for writing.").arg(path);
        }
        return false;
    }

    QString normalized = contents;
    if (!normalized.endsWith('\n')) {
        normalized.append('\n');
    }

    if (file.write(normalized.toUtf8()) < 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Unable to write `%1`.").arg(path);
        }
        return false;
    }

    if (!file.commit()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to finalize `%1`.").arg(path);
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
            *errorMessage = QStringLiteral("Unable to set permissions on `%1`.").arg(path);
        }
        return false;
    }

    return true;
}

bool runProcessAndCapture(const QString &program,
                          const QStringList &arguments,
                          const QString &workingDirectory,
                          QByteArray *output,
                          QString *errorMessage)
{
    QProcess process;
    process.setProgram(program);
    process.setArguments(arguments);
    process.setWorkingDirectory(workingDirectory);
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start();
    if (!process.waitForStarted()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to start `%1`.").arg(program);
        }
        return false;
    }

    if (!process.waitForFinished(-1)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Timed out while running `%1`.").arg(program);
        }
        return false;
    }

    const QByteArray mergedOutput = process.readAll();
    if (output) {
        *output = mergedOutput;
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (errorMessage) {
            const QString details = QString::fromLocal8Bit(mergedOutput).trimmed();
            *errorMessage = details.isEmpty()
                                ? QStringLiteral("`%1` failed.").arg(program)
                                : QStringLiteral("`%1` failed: %2").arg(program, details);
        }
        return false;
    }

    return true;
}

bool ensureDirectoryExists(const QString &path, QString *errorMessage)
{
    if (path.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Directory path is empty.");
        }
        return false;
    }

    const QFileInfo info(path);
    if (info.exists()) {
        if (info.isDir()) {
            return true;
        }
        if (errorMessage) {
            *errorMessage = QString("`%1` exists but is not a directory.").arg(path);
        }
        return false;
    }

    QDir directory;
    if (directory.mkpath(path)) {
        QFile::setPermissions(path,
                              QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
                                  | QFileDevice::ReadGroup | QFileDevice::ExeGroup
                                  | QFileDevice::ReadOther | QFileDevice::ExeOther);
        return true;
    }

    if (geteuid() == 0) {
        if (errorMessage) {
            *errorMessage = QString("Unable to create `%1`.").arg(path);
        }
        return false;
    }

    const QString sudoExecutable = QStandardPaths::findExecutable("sudo");
    const QString shExecutable = QStandardPaths::findExecutable("sh");
    if (sudoExecutable.isEmpty() || shExecutable.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QString("Unable to create `%1` and no non-interactive sudo path is available.").arg(path);
        }
        return false;
    }

    const QString command = QString("mkdir -p %1 && chown %2:%3 %1 && chmod 755 %1")
                                .arg(shellQuote(path),
                                     QString::number(getuid()),
                                     QString::number(getgid()));
    QProcess process;
    process.start(sudoExecutable, {"-n", shExecutable, "-c", command});
    if (!process.waitForFinished(10000) || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (errorMessage) {
            const QString details = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
            *errorMessage = details.isEmpty()
                                ? QString("Unable to create `%1`.").arg(path)
                                : QString("Unable to create `%1`: %2").arg(path, details);
        }
        return false;
    }

    if (QFileInfo(path).isDir()) {
        return true;
    }

    if (errorMessage) {
        *errorMessage = QString("Unable to create `%1`.").arg(path);
    }
    return false;
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
    const QString scriptsDirectory = findScriptsDirectory();
    const QString projectRoot = scriptsDirectory.isEmpty()
                                    ? QDir::currentPath()
                                    : QFileInfo(scriptsDirectory).absolutePath();
    workRootEdit_->setText(projectRoot);
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
    QString directoryError;
    if (!ensureDirectoryExists(workRoot, &directoryError)) {
        QMessageBox::critical(this, "Working directory error", directoryError);
        return;
    }

    currentRunDirectory_ = QDir(workRoot).filePath("run-" + runStamp);
    if (!ensureDirectoryExists(currentRunDirectory_, &directoryError)) {
        QMessageBox::critical(this, "Working directory error", directoryError);
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
    currentRuntimeScriptsDirectory_ = runtimeScriptsDirectory;
    currentSourceProjectRootDirectory_ = QFileInfo(scriptsDirectory).dir().absolutePath();
    currentInstallLogRootDirectory_ = QDir(QFileInfo(scriptsDirectory).dir().absolutePath()).filePath("logs");
    if (installScriptPaths_.isEmpty()) {
        QMessageBox::critical(this, "Install list empty", "No scripts were listed in `scripts/install.sh`.");
        return;
    }
    if (!ensureGenericDirectory(currentInstallLogRootDirectory_, &artifactError)) {
        QMessageBox::critical(this,
                              "Log directory error",
                              artifactError);
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
    currentInstallEntryName_.clear();
    mlfsDownloadScriptPaths_.clear();
    mlfsToolchainScriptPaths_.clear();
    mlfsArtifactsPrepared_ = false;
    currentInstallScriptIndex_ = 0;
    completedInstallSteps_ = 0;
    totalInstallSteps_ = installScriptPaths_.size();
    installInProgress_ = true;
    installCompleted_ = false;
    installShellSessionStarted_ = false;
    installSessionClosing_ = false;
    closeCurrentInstallLog();

    installLog_->clear();
    if (installProgressBar_) {
        if (totalInstallSteps_ > 0) {
            installProgressBar_->setRange(0, totalInstallSteps_);
            installProgressBar_->setValue(0);
        } else {
            installProgressBar_->setRange(0, 0);
        }
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
    currentInstallEntryName_.clear();
    currentRuntimeScriptsDirectory_.clear();
    currentInstallLogRootDirectory_.clear();
    currentInstallScriptIndex_ = -1;
    completedInstallSteps_ = 0;
    totalInstallSteps_ = 0;
    installShellSessionStarted_ = false;
    installSessionClosing_ = false;
    closeCurrentInstallLog();
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

    const auto setDirectoryPermissions = [errorMessage](const QString &path) -> bool {
        const QFileDevice::Permissions permissions = QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
                                                     | QFileDevice::ReadGroup | QFileDevice::ExeGroup
                                                     | QFileDevice::ReadOther | QFileDevice::ExeOther;
        if (!QFile::setPermissions(path, permissions)) {
            if (errorMessage) {
                *errorMessage = QString("Unable to set permissions on `%1`.").arg(path);
            }
            return false;
        }
        return true;
    };

    if (!setDirectoryPermissions(stagedRoot) || !setDirectoryPermissions(stagedScriptsDirectory)
        || !setDirectoryPermissions(stagedFilesDirectory)) {
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
                                QDir::Files | QDir::Readable | QDir::NoDotAndDotDot,
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
        if (!setDirectoryPermissions(QFileInfo(stagedPath).absolutePath())) {
            return false;
        }

        QFile sourceFile(sourcePath);
        if (!sourceFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            if (errorMessage) {
                *errorMessage = QString("Unable to read `%1`.").arg(sourceFile.fileName());
            }
            return false;
        }

        const QFileInfo sourceInfo(sourcePath);
        const bool executable = sourceInfo.permission(QFileDevice::ExeOwner)
                                || sourceInfo.permission(QFileDevice::ExeGroup)
                                || sourceInfo.permission(QFileDevice::ExeOther)
                                || sourceInfo.suffix() == QStringLiteral("sh");
        if (!writeFile(stagedPath, QString::fromUtf8(sourceFile.readAll()), executable)) {
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
        *runtimeScriptsDirectory = stagedScriptsDirectory;
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

        if (line == QLatin1String(kMlfsBookSentinel)) {
            scriptPaths.append(line);
            continue;
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

QString InstallerWindow::installScriptEntryName(const QString &scriptPath) const
{
    if (currentRuntimeScriptsDirectory_.isEmpty()) {
        return QFileInfo(scriptPath).fileName();
    }

    QString relativePath = QDir(currentRuntimeScriptsDirectory_).relativeFilePath(scriptPath);
    const QString generatedPrefix = QStringLiteral("mlfs-generated/");
    if (relativePath.startsWith(generatedPrefix)) {
        relativePath.remove(0, generatedPrefix.size());
    }
    return relativePath;
}

QString InstallerWindow::installLogPathForEntry(const QString &entryName) const
{
    QString relativePath = entryName;
    if (relativePath.endsWith(".sh")) {
        relativePath.chop(3);
    }
    relativePath += ".log";
    return QDir(currentInstallLogRootDirectory_).filePath(relativePath);
}

bool InstallerWindow::ensureGenericDirectory(const QString &path, QString *errorMessage) const
{
    const QFileInfo info(path);
    if (info.exists() && !info.isDir()) {
        if (errorMessage) {
            *errorMessage = QString("`%1` exists but is not a directory.").arg(path);
        }
        return false;
    }

    QDir directory;
    if (!directory.mkpath(path)) {
        if (errorMessage) {
            *errorMessage = QString("Unable to create directory `%1`.").arg(path);
        }
        return false;
    }

    setGenericPathPermissions(path, true, nullptr);

    return true;
}

bool InstallerWindow::setGenericPathPermissions(const QString &path, bool executable, QString *errorMessage) const
{
    const QFileDevice::Permissions permissions = executable
                                                     ? (QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
                                                        | QFileDevice::ReadGroup | QFileDevice::WriteGroup | QFileDevice::ExeGroup
                                                        | QFileDevice::ReadOther | QFileDevice::WriteOther | QFileDevice::ExeOther)
                                                     : (QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                                        | QFileDevice::ReadGroup | QFileDevice::WriteGroup
                                                        | QFileDevice::ReadOther | QFileDevice::WriteOther);
    if (!QFile::setPermissions(path, permissions)) {
        if (errorMessage) {
            *errorMessage = QString("Unable to set permissions on `%1`.").arg(path);
        }
        return false;
    }

    return true;
}

bool InstallerWindow::prepareCurrentInstallLog(QString *errorMessage)
{
    closeCurrentInstallLog();

    if (currentInstallEntryName_.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Install log path is empty.");
        }
        return false;
    }

    const QString relativeParent = QFileInfo(currentInstallEntryName_).path();
    QString relativeLogPath = currentInstallEntryName_;
    if (relativeLogPath.endsWith(".sh")) {
        relativeLogPath.chop(3);
    }
    relativeLogPath += ".log";
    const QString fallbackLogRootDirectory = QDir(currentRunDirectory_).filePath("logs");
    const QStringList candidateRoots = {currentInstallLogRootDirectory_, fallbackLogRootDirectory};

    QStringList failureReasons;
    for (const QString &logRootPath : candidateRoots) {
        QString rootError;
        if (!ensureGenericDirectory(logRootPath, &rootError)) {
            failureReasons.append(rootError);
            continue;
        }

        if (!relativeParent.isEmpty() && relativeParent != ".") {
            const QString absoluteParent = QDir(logRootPath).filePath(relativeParent);
            QString parentError;
            if (!ensureGenericDirectory(absoluteParent, &parentError)) {
                failureReasons.append(parentError);
                continue;
            }
        }

        const QString logPath = QDir(logRootPath).filePath(relativeLogPath);
        currentInstallLogFile_.setFileName(logPath);
        if (!currentInstallLogFile_.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            failureReasons.append(QString("unable to open `%1` for writing").arg(logPath));
            currentInstallLogFile_.setFileName(QString());
            continue;
        }
        setGenericPathPermissions(logPath, false, nullptr);

        const QString header = QString("# Script: %1\n# Started: %2\n# Log Path: %3\n\n")
                                   .arg(currentInstallEntryName_,
                                        QDateTime::currentDateTime().toString(Qt::ISODate),
                                        logPath);
        currentInstallLogFile_.write(header.toUtf8());
        currentInstallLogFile_.flush();

        if (logRootPath != currentInstallLogRootDirectory_) {
            appendInstallLogLine(QString("> project logs unavailable; using `%1`").arg(logPath));
        }

        return true;
    }

    if (errorMessage) {
        *errorMessage = QString("Unable to prepare install log for `%1`: %2")
                            .arg(currentInstallEntryName_, failureReasons.join("; "));
    }
    return false;
}

void InstallerWindow::closeCurrentInstallLog()
{
    if (currentInstallLogFile_.isOpen()) {
        currentInstallLogFile_.flush();
        currentInstallLogFile_.close();
    }
    currentInstallLogFile_.setFileName(QString());
}

void InstallerWindow::appendCurrentInstallLogLine(const QString &line)
{
    if (!currentInstallLogFile_.isOpen()) {
        return;
    }

    currentInstallLogFile_.write(line.toUtf8());
    currentInstallLogFile_.write("\n");
    currentInstallLogFile_.flush();
}

bool InstallerWindow::prepareMlfsBookArtifacts(QString *errorMessage)
{
    if (mlfsArtifactsPrepared_) {
        return true;
    }

    const QString gitExecutable = QStandardPaths::findExecutable(QStringLiteral("git"));
    const QString bashExecutable = QStandardPaths::findExecutable(QStringLiteral("bash"));
    const QString xsltprocExecutable = QStandardPaths::findExecutable(QStringLiteral("xsltproc"));
    const QString xmllintExecutable = QStandardPaths::findExecutable(QStringLiteral("xmllint"));
    if (gitExecutable.isEmpty() || bashExecutable.isEmpty() || xsltprocExecutable.isEmpty() || xmllintExecutable.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("MLFS generation requires `git`, `bash`, `xsltproc`, and `xmllint` in PATH.");
        }
        return false;
    }

    const QString generatedRoot = QDir(currentRuntimeScriptsDirectory_).filePath(QStringLiteral("mlfs-generated"));
    const QString bookSourceDirectory = QDir(currentRunDirectory_).filePath(QStringLiteral("mlfs-book-source"));
    const QString bookWorkDirectory = QDir(currentRunDirectory_).filePath(QStringLiteral("mlfs-book-work"));
    const QString profiledXmlPath = QDir(bookWorkDirectory).filePath(QStringLiteral("book-profiled.xml"));
    const QString fullXmlPath = QDir(bookWorkDirectory).filePath(QStringLiteral("book-full.xml"));
    const QString wgetListPath = QDir(bookWorkDirectory).filePath(QStringLiteral("wget-list"));
    const QString md5ListPath = QDir(bookWorkDirectory).filePath(QStringLiteral("md5sums"));

    if (QFileInfo(generatedRoot).exists() && !QDir(generatedRoot).removeRecursively()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Unable to clear `%1`.").arg(generatedRoot);
        }
        return false;
    }
    if (QFileInfo(bookWorkDirectory).exists() && !QDir(bookWorkDirectory).removeRecursively()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Unable to clear `%1`.").arg(bookWorkDirectory);
        }
        return false;
    }
    if (QFileInfo(bookSourceDirectory).exists() && !QDir(bookSourceDirectory).removeRecursively()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Unable to clear `%1`.").arg(bookSourceDirectory);
        }
        return false;
    }

    QDir directory;
    if (!directory.mkpath(generatedRoot) || !directory.mkpath(bookWorkDirectory)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Unable to create the MLFS generation workspace.");
        }
        return false;
    }

    QByteArray processOutput;
    appendInstallLogLine(QStringLiteral("> cloning MLFS book source"));
    if (!runProcessAndCapture(gitExecutable,
                              {QStringLiteral("clone"),
                               QStringLiteral("--depth"),
                               QStringLiteral("1"),
                               QStringLiteral("--branch"),
                               QString::fromLatin1(kMlfsBookBranch),
                               QStringLiteral("--single-branch"),
                               QString::fromLatin1(kMlfsBookRepositoryUrl),
                               bookSourceDirectory},
                              currentRunDirectory_,
                              &processOutput,
                              errorMessage)) {
        return false;
    }
    if (!processOutput.trimmed().isEmpty()) {
        appendInstallLogLine(QString::fromLocal8Bit(processOutput).trimmed());
    }
    appendInstallLogLine(QStringLiteral("> using MLFS branch `%1` from `%2`")
                             .arg(QString::fromLatin1(kMlfsBookBranch),
                                  QString::fromLatin1(kMlfsBookRepositoryUrl)));

    appendInstallLogLine(QStringLiteral("> processing MLFS book scripts"));
    if (!runProcessAndCapture(bashExecutable,
                              {QStringLiteral("process-scripts.sh")},
                              bookSourceDirectory,
                              &processOutput,
                              errorMessage)) {
        return false;
    }

    appendInstallLogLine(QStringLiteral("> generating MLFS conditional entities"));
    if (!runProcessAndCapture(bashExecutable,
                              {QStringLiteral("git-version.sh"),
                               QString::fromLatin1(kMlfsProfileRevision)},
                              bookSourceDirectory,
                              &processOutput,
                              errorMessage)) {
        return false;
    }

    appendInstallLogLine(QStringLiteral("> profiling MLFS book XML"));
    if (!runProcessAndCapture(xsltprocExecutable,
                              {QStringLiteral("--nonet"),
                               QStringLiteral("--xinclude"),
                               QStringLiteral("--stringparam"),
                               QStringLiteral("profile.revision"),
                               QString::fromLatin1(kMlfsProfileRevision),
                               QStringLiteral("--output"),
                               profiledXmlPath,
                               QStringLiteral("stylesheets/lfs-xsl/profile.xsl"),
                               QStringLiteral("index.xml")},
                              bookSourceDirectory,
                              &processOutput,
                              errorMessage)) {
        return false;
    }

    appendInstallLogLine(QStringLiteral("> validating profiled MLFS XML"));
    if (!runProcessAndCapture(xmllintExecutable,
                              {QStringLiteral("--nonet"),
                               QStringLiteral("--encode"),
                               QStringLiteral("UTF-8"),
                               QStringLiteral("--postvalid"),
                               QStringLiteral("--output"),
                               fullXmlPath,
                               profiledXmlPath},
                              bookSourceDirectory,
                              &processOutput,
                              errorMessage)) {
        return false;
    }

    appendInstallLogLine(QStringLiteral("> generating MLFS source manifests"));
    if (!runProcessAndCapture(xsltprocExecutable,
                              {QStringLiteral("--output"),
                               wgetListPath,
                               QStringLiteral("stylesheets/wget-list.xsl"),
                               fullXmlPath},
                              bookSourceDirectory,
                              &processOutput,
                              errorMessage)) {
        return false;
    }
    if (!runProcessAndCapture(xsltprocExecutable,
                              {QStringLiteral("--output"),
                               md5ListPath,
                               QStringLiteral("stylesheets/md5sum.xsl"),
                               fullXmlPath},
                              bookSourceDirectory,
                              &processOutput,
                              errorMessage)) {
        return false;
    }

    QFile xmlFile(fullXmlPath);
    if (!xmlFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Unable to read `%1`.").arg(fullXmlPath);
        }
        return false;
    }

    QDomDocument bookDocument;
    const QDomDocument::ParseResult parseResult =
        bookDocument.setContent(&xmlFile, QDomDocument::ParseOption::UseNamespaceProcessing);
    if (!parseResult) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Unable to parse `%1` at %2:%3: %4")
                                .arg(fullXmlPath)
                                .arg(parseResult.errorLine)
                                .arg(parseResult.errorColumn)
                                .arg(parseResult.errorMessage);
        }
        return false;
    }

    mlfsDownloadScriptPaths_.clear();
    mlfsToolchainScriptPaths_.clear();

    const QString downloadScriptRelativePath = QStringLiteral("mlfs-generated/chapter3/download-sources.sh");
    const QString downloadScriptAbsolutePath = QDir(currentRuntimeScriptsDirectory_).filePath(downloadScriptRelativePath);
    if (!directory.mkpath(QFileInfo(downloadScriptAbsolutePath).absolutePath())) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Unable to create `%1`.").arg(QFileInfo(downloadScriptAbsolutePath).absolutePath());
        }
        return false;
    }
    if (!writeGeneratedTextFile(downloadScriptAbsolutePath,
                                buildMlfsDownloadScript(wgetListPath, md5ListPath),
                                true,
                                errorMessage)) {
        return false;
    }
    mlfsDownloadScriptPaths_.append(downloadScriptAbsolutePath);

    const DriveInfo drive = currentDrive();
    const QVector<PlannedPartition> partitions = collectPartitions();
    const QString hostname = hostnameEdit_->text().trimmed().isEmpty() ? QStringLiteral("lfs")
                                                                       : hostnameEdit_->text().trimmed();
    const QString fqdn = hostname + QStringLiteral(".localdomain");
    const QString timezone = timeZoneCombo_->currentText().trimmed();
    const QString defaultDrivePath = drive.path.isEmpty() ? QStringLiteral("/dev/sda") : drive.path;
    const QRegularExpression unresolvedPlaceholderPattern(QStringLiteral(R"(<[^>\n]+>)"));
    const auto partitionIndexForMountPoint = [&partitions](const QString &mountPoint) -> int {
        for (int index = 0; index < partitions.size(); ++index) {
            if (partitions.at(index).mountPoint == mountPoint) {
                return index;
            }
        }
        return -1;
    };
    const int rootPartitionIndex = partitionIndexForMountPoint(QStringLiteral("/"));
    const int bootPartitionIndex = partitionIndexForMountPoint(QStringLiteral("/boot"));
    const int efiPartitionIndex = partitionIndexForMountPoint(QStringLiteral("/boot/efi"));
    const QString rootPartitionNode = rootPartitionIndex >= 0 ? partitionNodeName(defaultDrivePath, rootPartitionIndex)
                                                              : QStringLiteral("/dev/sda2");
    const QString rootFileSystem = rootPartitionIndex >= 0 ? fstabFileSystem(partitions.at(rootPartitionIndex))
                                                           : QStringLiteral("ext4");
    const int grubPartitionIndex = bootPartitionIndex >= 0 ? bootPartitionIndex : rootPartitionIndex;
    const QString grubRootSpecifier = QStringLiteral("(hd0,%1)").arg(grubPartitionIndex >= 0 ? grubPartitionIndex + 1 : 2);

    QString detectedKernelVersion;
    const auto buildHostsCommand = [&hostname, &fqdn]() -> QString {
        QStringList lines;
        lines << QStringLiteral("cat > /etc/hosts <<'EOF'");
        lines << QStringLiteral("# Begin /etc/hosts");
        lines << QString();
        lines << QStringLiteral("127.0.0.1 localhost.localdomain localhost");
        lines << QStringLiteral("127.0.1.1 %1 %2").arg(fqdn, hostname);
        lines << QStringLiteral("::1       localhost ip6-localhost ip6-loopback");
        lines << QStringLiteral("ff02::1   ip6-allnodes");
        lines << QStringLiteral("ff02::2   ip6-allrouters");
        lines << QString();
        lines << QStringLiteral("# End /etc/hosts");
        lines << QStringLiteral("EOF");
        return lines.join('\n');
    };
    const auto buildGrubConfigCommand = [&]() -> QString {
        const QString kernelVersion = detectedKernelVersion.isEmpty() ? QStringLiteral("linux") : detectedKernelVersion;
        const QString kernelPath = bootPartitionIndex >= 0 ? QStringLiteral("/vmlinuz-%1").arg(kernelVersion)
                                                           : QStringLiteral("/boot/vmlinuz-%1").arg(kernelVersion);

        QStringList lines;
        lines << QStringLiteral("cat > /boot/grub/grub.cfg <<'EOF'");
        lines << QStringLiteral("# Begin /boot/grub/grub.cfg");
        lines << QStringLiteral("set default=0");
        lines << QStringLiteral("set timeout=5");
        lines << QString();
        lines << QStringLiteral("insmod part_gpt");
        lines << QStringLiteral("insmod ext2");
        lines << QString();
        lines << QStringLiteral("set root=%1").arg(grubRootSpecifier);
        lines << QString();
        lines << QStringLiteral("# For UEFI");
        lines << QStringLiteral("insmod efi_gop");
        lines << QStringLiteral("insmod efi_uga");
        lines << QString();
        lines << QStringLiteral("set gfxpayload=1024x768x32");
        lines << QString();
        lines << QStringLiteral("menuentry \"GNU/Linux, Linux %1\" {").arg(kernelVersion);
        lines << QStringLiteral("        linux   %1 root=%2 ro").arg(kernelPath, rootPartitionNode);
        lines << QStringLiteral("}");
        lines << QStringLiteral("EOF");
        return lines.join('\n');
    };
    const auto rewriteMlfsCommand = [&](QString command,
                                        const QString &sectionId,
                                        bool noDump) -> QString {
        command = command.trimmed();
        if (command.isEmpty()) {
            return {};
        }

        if (sectionId == QStringLiteral("ch-bootable-fstab") || sectionId == QStringLiteral("ch-config-clock")) {
            return {};
        }

        if (noDump) {
            const bool allowedBoundary = command.startsWith(QStringLiteral("su - "))
                                         || command.startsWith(QStringLiteral("chroot \"$LFS\""))
                                         || command == QStringLiteral("exit")
                                         || command == QStringLiteral("exec /usr/bin/bash --login")
                                         || (sectionId == QStringLiteral("ch-bootable-grub")
                                             && command.startsWith(QStringLiteral("grub-install ")));
            if (!allowedBoundary) {
                return {};
            }
        }

        if (sectionId == QStringLiteral("ch-config-network")) {
            if (command.contains(QStringLiteral("/etc/systemd/network/"))
                || command.contains(QStringLiteral("/etc/resolv.conf"))
                || command.startsWith(QStringLiteral("systemctl disable "))) {
                return {};
            }
            if (command.contains(QStringLiteral("/etc/hostname"))) {
                return QStringLiteral("echo %1 > /etc/hostname").arg(shellQuote(hostname));
            }
            if (command.startsWith(QStringLiteral("cat > /etc/hosts <<"))) {
                return buildHostsCommand();
            }
        }

        if (sectionId == QStringLiteral("ch-config-locale")) {
            command.replace(QStringLiteral("<ll>_<CC>.<charmap><@modifiers>"), QStringLiteral("en_US.UTF-8"));
            if (command.startsWith(QStringLiteral("localectl "))) {
                return {};
            }
        }

        if (sectionId == QStringLiteral("ch-bootable-grub")) {
            if (command.startsWith(QStringLiteral("grub-install "))) {
                if (efiPartitionIndex >= 0) {
                    return QStringLiteral("grub-install --target=x86_64-efi --removable");
                }
                return QStringLiteral("grub-install %1 --target=i386-pc").arg(shellQuote(defaultDrivePath));
            }
            if (command.startsWith(QStringLiteral("cat > /boot/grub/grub.cfg <<"))) {
                return buildGrubConfigCommand();
            }
        }

        command.replace(QStringLiteral("<lfs>"), hostname);
        command.replace(QStringLiteral("<HOSTNAME>"), hostname);
        command.replace(QStringLiteral("<FQDN>"), fqdn);
        command.replace(QStringLiteral("<Your Domain Name>"), QStringLiteral("localdomain"));
        command.replace(QStringLiteral("<ll>_<CC>.<charmap><@modifiers>"), QStringLiteral("en_US.UTF-8"));
        command.replace(QStringLiteral("TIMEZONE"), timezone);

        if (command.startsWith(QStringLiteral("passwd "))
            || command.startsWith(QStringLiteral("timedatectl "))
            || command.startsWith(QStringLiteral("locale -a"))
            || command.startsWith(QStringLiteral("LC_ALL="))
            || command.startsWith(QStringLiteral("grub-mkrescue"))
            || command.startsWith(QStringLiteral("xorriso "))
            || command.startsWith(QStringLiteral("efibootmgr "))
            || command.startsWith(QStringLiteral("mountpoint /sys/firmware/efi/efivars"))
            || command == QStringLiteral("mount /boot")
            || command.startsWith(QStringLiteral("umount "))
            || command.startsWith(QStringLiteral("make menuconfig"))
            || command.startsWith(QStringLiteral("systemctl disable "))
            || command.startsWith(QStringLiteral("vim -c"))) {
            return {};
        }

        if (unresolvedPlaceholderPattern.match(command).hasMatch()) {
            return {};
        }

        return command;
    };

    const QDomElement root = bookDocument.documentElement();
    QList<QDomElement> bookChapters;
    for (const QDomElement &part : directChildElements(root, QStringLiteral("part"))) {
        const QList<QDomElement> chaptersInPart = directChildElements(part, QStringLiteral("chapter"));
        for (const QDomElement &chapter : chaptersInPart) {
            bookChapters.append(chapter);
        }
    }
    if (bookChapters.isEmpty()) {
        bookChapters = directChildElements(root, QStringLiteral("chapter"));
    }

    int chapterNumber = 0;
    for (const QDomElement &chapter : bookChapters) {
        ++chapterNumber;
        if (chapterNumber < 4 || chapterNumber > 10) {
            continue;
        }

        int sectionNumber = 0;
        for (const QDomElement &section : directChildElements(chapter, QStringLiteral("sect1"))) {
            ++sectionNumber;
            const QString sectionId = section.attribute(QStringLiteral("id"));
            const QString title = sectionTitle(section);
            if (sectionId == QStringLiteral("ch-bootable-fstab")) {
                continue;
            }
            if (sectionId == QStringLiteral("ch-bootable-kernel") && title.startsWith(QStringLiteral("Linux-"))) {
                detectedKernelVersion = title.mid(QStringLiteral("Linux-").size());
            }

            QList<MlfsCommand> rawCommands;
            collectMlfsCommands(section, false, &rawCommands);

            QStringList commands;
            for (const MlfsCommand &rawCommand : rawCommands) {
                const QString rewritten = rewriteMlfsCommand(rawCommand.text, sectionId, rawCommand.noDump);
                if (!rewritten.trimmed().isEmpty()) {
                    commands.append(rewritten.trimmed());
                }
            }
            if (sectionId == QStringLiteral("ch-bootable-kernel") && !commands.isEmpty()) {
                const int insertIndex = commands.constFirst().startsWith(QStringLiteral("make mrproper")) ? 1 : 0;
                commands.insert(insertIndex, QStringLiteral("if [ ! -f .config ]; then make defconfig; fi"));
            }
            if (commands.isEmpty()) {
                continue;
            }

            QString slug = slugify(title);
            if (slug.isEmpty()) {
                slug = QStringLiteral("section-%1").arg(sectionNumber, 3, 10, QChar('0'));
            }

            QString setupCommands;
            QString cleanupCommands;
            if (section.attribute(QStringLiteral("role")) == QStringLiteral("wrap")) {
                const QList<QDomElement> infoElements = directChildElements(section, QStringLiteral("sect1info"));
                if (!infoElements.isEmpty()) {
                    const QList<QDomElement> addressElements = directChildElements(infoElements.constFirst(), QStringLiteral("address"));
                    if (!addressElements.isEmpty()) {
                        const QString archiveBase = archiveBaseNameFromUrl(addressElements.constFirst().text().trimmed());
                        if (!archiveBase.isEmpty()) {
                            QStringList setupLines;
                            setupLines << QStringLiteral("cd \"$LFS/sources\"");
                            setupLines << QStringLiteral("find . -maxdepth 1 -mindepth 1 -type d -name %1 -exec rm -rf {} +")
                                              .arg(shellQuote(archiveBase + QStringLiteral("*")));
                            setupLines << QStringLiteral("sh autountar %1").arg(shellQuote(archiveBase));
                            setupLines << QStringLiteral("source_dir=$(find . -maxdepth 1 -mindepth 1 -type d -name %1 | sort | head -n 1)")
                                              .arg(shellQuote(archiveBase + QStringLiteral("*")));
                            setupLines << QStringLiteral("if [ -z \"$source_dir\" ]; then echo \"Unable to find extracted source for %1\" >&2; exit 1; fi")
                                              .arg(archiveBase);
                            setupLines << QStringLiteral("cd \"$source_dir\"");
                            setupCommands = setupLines.join('\n');

                            QStringList cleanupLines;
                            cleanupLines << QStringLiteral("cd \"$LFS/sources\"");
                            cleanupLines << QStringLiteral("find . -maxdepth 1 -mindepth 1 -type d -name %1 -exec rm -rf {} +")
                                                .arg(shellQuote(archiveBase + QStringLiteral("*")));
                            cleanupCommands = cleanupLines.join('\n');
                        }
                    }
                }
            }

            const QString relativePath = generatedMlfsScriptPath(chapterNumber, sectionNumber, slug);
            const QString absolutePath = QDir(currentRuntimeScriptsDirectory_).filePath(relativePath);
            if (!directory.mkpath(QFileInfo(absolutePath).absolutePath())) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral("Unable to create `%1`.").arg(QFileInfo(absolutePath).absolutePath());
                }
                return false;
            }
            if (!writeGeneratedTextFile(absolutePath,
                                        generatedMlfsScriptBody(title.isEmpty() ? slug : title,
                                                                commands,
                                                                setupCommands,
                                                                cleanupCommands),
                                        true,
                                        errorMessage)) {
                return false;
            }
            mlfsToolchainScriptPaths_.append(absolutePath);
        }
    }

    if (mlfsToolchainScriptPaths_.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("The MLFS book did not produce any runnable generated scripts.");
        }
        return false;
    }

    mlfsArtifactsPrepared_ = true;
    appendInstallLogLine(QStringLiteral("> prepared %1 MLFS book scripts").arg(mlfsToolchainScriptPaths_.size()));
    return true;
}

bool InstallerWindow::expandMlfsSentinelEntry(const QString &sentinel, QString *errorMessage)
{
    if (!prepareMlfsBookArtifacts(errorMessage)) {
        return false;
    }

    QStringList replacements;
    if (sentinel == QLatin1String(kMlfsBookSentinel)) {
        replacements = mlfsDownloadScriptPaths_;
        replacements.append(mlfsToolchainScriptPaths_);
    }
    if (replacements.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("No generated scripts are available for `%1`.").arg(sentinel);
        }
        return false;
    }

    installScriptPaths_.removeAt(currentInstallScriptIndex_);
    for (int index = replacements.size() - 1; index >= 0; --index) {
        installScriptPaths_.insert(currentInstallScriptIndex_, replacements.at(index));
    }

    totalInstallSteps_ += replacements.size() - 1;
    if (installProgressBar_) {
        installProgressBar_->setRange(0, qMax(totalInstallSteps_, 1));
        installProgressBar_->setValue(completedInstallSteps_);
    }

    appendInstallLogLine(QStringLiteral("> expanded `%1` into %2 generated script(s)")
                             .arg(sentinel)
                             .arg(replacements.size()));
    return true;
}

QString InstallerWindow::buildInstallSessionPrelude() const
{
    const QString stagedRoot = QFileInfo(currentRuntimeScriptsDirectory_).dir().absolutePath();

    QStringList lines;
    lines << "unset BASH_ENV ENV";
    lines << QString("PROJECT_ROOT=%1").arg(shellQuote(stagedRoot));
    lines << "export PROJECT_ROOT";
    lines << QString("SOURCE_PROJECT_ROOT=%1").arg(shellQuote(currentSourceProjectRootDirectory_));
    lines << "export SOURCE_PROJECT_ROOT";
    lines << QString("INSTALL_RUN_DIR=%1").arg(shellQuote(currentRunDirectory_));
    lines << "export INSTALL_RUN_DIR";

    return lines.join('\n') + '\n';
}

bool InstallerWindow::queueInstallCommand(const QString &command, const QString &context, QString *errorMessage)
{
    if (!installProcess_ || installProcess_->state() != QProcess::Running) {
        if (errorMessage) {
            *errorMessage = QString("Install shell is not running for `%1`.").arg(context);
        }
        return false;
    }

    if (installProcess_->write(command.toUtf8()) < 0) {
        if (errorMessage) {
            *errorMessage = QString("Unable to send `%1` to the install shell.").arg(context);
        }
        return false;
    }

    return true;
}

bool InstallerWindow::queueInstallScriptChunk(const QString &scriptPath, QString *errorMessage)
{
    QFile scriptFile(scriptPath);
    if (!scriptFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QString("Unable to read `%1`.").arg(scriptPath);
        }
        return false;
    }

    QString scriptContents = QString::fromUtf8(scriptFile.readAll());
    if (!scriptContents.endsWith('\n')) {
        scriptContents.append('\n');
    }
    scriptContents.chop(1);

    const QString entryName = installScriptEntryName(scriptPath);
    bool inlineDoneMarker = false;
    scriptContents = rewriteInteractiveHandoffs(scriptContents, entryName, &inlineDoneMarker);
    QStringList lines;
    lines << QString("echo %1").arg(shellQuote("__SCRIPT_BEGIN__:" + entryName));
    lines << "set -euo pipefail";
    lines << installShellHandoffHelpers().trimmed();
    lines << "set -x";
    lines << scriptContents;
    if (!inlineDoneMarker) {
        lines << "set +x";
        lines << QString("echo %1").arg(shellQuote("__SCRIPT_DONE__:" + entryName));
    }

    return queueInstallCommand(lines.join('\n') + '\n', entryName, errorMessage);
}

bool InstallerWindow::startInstallShellSession(QString *errorMessage)
{
    if (installShellSessionStarted_) {
        return true;
    }

    const QString bashExecutable = QStandardPaths::findExecutable("bash");
    if (bashExecutable.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Unable to find `bash` in PATH.");
        }
        return false;
    }

    const QString scriptExecutable = QStandardPaths::findExecutable("script");
    if (scriptExecutable.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Persistent terminal mode requires `script` in PATH.");
        }
        return false;
    }

    const QString stagedRoot = QFileInfo(currentRuntimeScriptsDirectory_).dir().absolutePath();
    installProcess_->setWorkingDirectory(stagedRoot);

    QProcessEnvironment processEnvironment = QProcessEnvironment::systemEnvironment();
    processEnvironment.remove("BASH_ENV");
    processEnvironment.remove("ENV");
    processEnvironment.insert("INSTALL_RUN_DIR", currentRunDirectory_);
    installProcess_->setProcessEnvironment(processEnvironment);

    const QString ptyCommand = QString("%1 --noprofile --norc").arg(shellQuote(bashExecutable));

    QString program;
    QStringList arguments;
    if (geteuid() != 0) {
        const QString sudoExecutable = QStandardPaths::findExecutable("sudo");
        const QString envExecutable = QStandardPaths::findExecutable("env");
        if (sudoExecutable.isEmpty() || envExecutable.isEmpty()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Root access requires `sudo` and `env` in PATH.");
            }
            return false;
        }

        program = sudoExecutable;
        arguments = {
            "-n",
            envExecutable,
            QString("INSTALL_RUN_DIR=%1").arg(currentRunDirectory_),
            "BASH_ENV=",
            "ENV=",
            scriptExecutable,
            "-qefc",
            ptyCommand,
            "/dev/null"
        };
    } else {
        program = scriptExecutable;
        arguments = {"-qefc", ptyCommand, "/dev/null"};
    }

    installProcess_->start(program, arguments);
    if (!installProcess_->waitForStarted(5000)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to start the persistent install shell.");
        }
        return false;
    }

    installShellSessionStarted_ = true;
    installSessionClosing_ = false;
    return queueInstallCommand(buildInstallSessionPrelude(), QStringLiteral("install session setup"), errorMessage);
}

void InstallerWindow::startNextInstallScript()
{
    if (!installProcess_) {
        return;
    }

    if (currentInstallScriptIndex_ < 0) {
        return;
    }

    QString errorMessage;
    if (!startInstallShellSession(&errorMessage)) {
        installInProgress_ = false;
        installCompleted_ = false;
        appendInstallLogLine(QString("> %1").arg(errorMessage));
        setInstallStatus("Current Step: Failed", QColor("#b71c1c"));
        updateNavigationState();
        return;
    }

    if (currentInstallScriptIndex_ >= installScriptPaths_.size()) {
        if (!installSessionClosing_) {
            installSessionClosing_ = true;
            currentInstallScriptPath_.clear();
            currentInstallEntryName_.clear();
            closeCurrentInstallLog();
            pendingInstallStepText_.clear();
            setInstallStatus("Current Step: Finalizing shell session", QColor("#1b5e20"));
            if (!queueInstallCommand(QStringLiteral("exit\nexit\nexit\n"),
                                     QStringLiteral("install session shutdown"),
                                     &errorMessage)) {
                installInProgress_ = false;
                installCompleted_ = false;
                appendInstallLogLine(QString("> %1").arg(errorMessage));
                setInstallStatus("Current Step: Failed", QColor("#b71c1c"));
                updateNavigationState();
            }
        }
        return;
    }

    const QString scriptPath = installScriptPaths_.at(currentInstallScriptIndex_);
    if (scriptPath == QLatin1String(kMlfsBookSentinel)) {
        setInstallStatus("Current Step: Preparing MLFS book scripts", QColor("#1b5e20"));
        if (!expandMlfsSentinelEntry(scriptPath, &errorMessage)) {
            installInProgress_ = false;
            installCompleted_ = false;
            appendInstallLogLine(QString("> %1").arg(errorMessage));
            setInstallStatus("Current Step: Failed", QColor("#b71c1c"));
            updateNavigationState();
            return;
        }

        startNextInstallScript();
        return;
    }

    const QFileInfo scriptInfo(scriptPath);
    if (!scriptInfo.exists() || !scriptInfo.isFile() || !scriptInfo.isReadable()) {
        installInProgress_ = false;
        installCompleted_ = false;
        appendInstallLogLine(QString("> script unavailable: %1").arg(scriptPath));
        setInstallStatus("Current Step: Failed", QColor("#b71c1c"));
        updateNavigationState();
        return;
    }

    currentInstallScriptPath_ = scriptPath;
    currentInstallEntryName_ = installScriptEntryName(scriptPath);
    installOutputBuffer_.clear();
    pendingInstallStepText_.clear();
    if (!prepareCurrentInstallLog(&errorMessage)) {
        installInProgress_ = false;
        installCompleted_ = false;
        appendInstallLogLine(QString("> %1").arg(errorMessage));
        setInstallStatus("Current Step: Failed", QColor("#b71c1c"));
        updateNavigationState();
        return;
    }

    if (!queueInstallScriptChunk(scriptPath, &errorMessage)) {
        installInProgress_ = false;
        installCompleted_ = false;
        appendCurrentInstallLogLine(QString("> %1").arg(errorMessage));
        appendInstallLogLine(QString("> %1").arg(errorMessage));
        setInstallStatus("Current Step: Failed", QColor("#b71c1c"));
        updateNavigationState();
        return;
    }

    setInstallStatus(QString("Current Step: Running %1").arg(currentInstallEntryName_), QColor("#1b5e20"));
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
        closeCurrentInstallLog();
        installShellSessionStarted_ = false;
        installSessionClosing_ = false;
        return;
    }

    const QString scriptName = !currentInstallEntryName_.isEmpty()
                                   ? currentInstallEntryName_
                                   : (currentInstallScriptPath_.isEmpty()
                                          ? QStringLiteral("<unknown>")
                                          : QFileInfo(currentInstallScriptPath_).fileName());

    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        installInProgress_ = false;
        installCompleted_ = false;
        const QString statusText = exitStatus == QProcess::NormalExit ? QStringLiteral("normal-exit")
                                                                      : QStringLiteral("crashed");
        const QString failureLine = QString("> %1 failed with exit code %2 (%3)")
                                        .arg(scriptName)
                                        .arg(exitCode)
                                        .arg(statusText);
        appendCurrentInstallLogLine(failureLine);
        appendInstallLogLine(failureLine);
        closeCurrentInstallLog();
        installShellSessionStarted_ = false;
        installSessionClosing_ = false;
        setInstallStatus("Current Step: Failed", QColor("#b71c1c"));
        updateNavigationState();
        return;
    }

    closeCurrentInstallLog();
    installShellSessionStarted_ = false;

    if (installSessionClosing_ && currentInstallScriptIndex_ >= installScriptPaths_.size()) {
        installSessionClosing_ = false;
        installInProgress_ = false;
        installCompleted_ = true;
        currentInstallScriptPath_.clear();
        currentInstallEntryName_.clear();
        if (installProgressBar_) {
            if (totalInstallSteps_ > 0) {
                installProgressBar_->setValue(totalInstallSteps_);
            } else {
                installProgressBar_->setRange(0, 1);
                installProgressBar_->setValue(1);
            }
        }
        setInstallStatus("Current Step: Complete", QColor("#1b5e20"));
        updateNavigationState();
        return;
    }

    installInProgress_ = false;
    installCompleted_ = false;
    appendInstallLogLine(QString("> install shell exited unexpectedly while running %1").arg(scriptName));
    setInstallStatus("Current Step: Failed", QColor("#b71c1c"));
    updateNavigationState();
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
        const QString errorLine = QString("> %1 process error: %2").arg(scriptName, installProcess_->errorString());
        appendCurrentInstallLogLine(errorLine);
        appendInstallLogLine(errorLine);
        closeCurrentInstallLog();
        installShellSessionStarted_ = false;
        installSessionClosing_ = false;
        setInstallStatus("Current Step: Failed", QColor("#b71c1c"));
        updateNavigationState();
        return;
    }

    const QString warningLine = QString("> %1 process warning: %2").arg(scriptName, installProcess_->errorString());
    appendCurrentInstallLogLine(warningLine);
    appendInstallLogLine(warningLine);
}

void InstallerWindow::appendInstallLogLine(const QString &line)
{
    if (!installLog_) {
        return;
    }

    QScrollBar *scrollBar = installLog_->verticalScrollBar();
    const bool followTail = !scrollBar || scrollBar->value() >= scrollBar->maximum() - 1;
    installLog_->appendPlainText(line);
    if (followTail && scrollBar) {
        scrollBar->setValue(scrollBar->maximum());
    }
}

void InstallerWindow::processInstallOutputLine(const QString &line)
{
    if (line.isEmpty()) {
        return;
    }

    if (line.startsWith("__SCRIPT_BEGIN__:")) {
        currentInstallEntryName_ = line.mid(QString("__SCRIPT_BEGIN__:").size()).trimmed();
        return;
    }

    if (line.startsWith("__SCRIPT_DONE__:")) {
        currentInstallEntryName_ = line.mid(QString("__SCRIPT_DONE__:").size()).trimmed();
        if (installProgressBar_ && totalInstallSteps_ > 0) {
            ++completedInstallSteps_;
            installProgressBar_->setValue(qMin(completedInstallSteps_, totalInstallSteps_));
        }
        closeCurrentInstallLog();
        currentInstallScriptPath_.clear();
        ++currentInstallScriptIndex_;
        startNextInstallScript();
        return;
    }

    static const QRegularExpression scriptMarkerEchoPattern(R"(^\+\s+echo\s+['"]?__SCRIPT_(BEGIN|DONE)__:.*$)");
    if (scriptMarkerEchoPattern.match(line).hasMatch()) {
        return;
    }

    QString displayLine = line;
    static const QRegularExpression ansiEscapePattern(QStringLiteral(R"(\x1B\[[0-9;?]*[ -/]*[@-~])"));
    displayLine.remove(ansiEscapePattern);
    displayLine.replace('\t', "    ");

    if (displayLine.contains("__SCRIPT_BEGIN__:") || displayLine.contains("__SCRIPT_DONE__:")) {
        return;
    }

    QString sanitizedLine;
    sanitizedLine.reserve(displayLine.size());
    for (const QChar character : std::as_const(displayLine)) {
        if (character == QChar::fromLatin1('\b') ||
            character == QChar::fromLatin1('\f') ||
            character == QChar::fromLatin1('\v')) {
            continue;
        }

        if (character.isPrint() || character == QChar::fromLatin1(' ')) {
            sanitizedLine.append(character);
        }
    }

    static const QRegularExpression driverEchoedCommandPattern(
        R"(^(?:.*[#$]\s+)?(?:set\s+-euo\s+pipefail|set\s+-x|set\s+\+x|echo\s+['"]?__SCRIPT_(?:BEGIN|DONE)__:.*|unset\s+BASH_ENV\s+ENV|PROJECT_ROOT=.*|export\s+PROJECT_ROOT|INSTALL_RUN_DIR=.*|export\s+INSTALL_RUN_DIR)\s*$)");
    if (driverEchoedCommandPattern.match(sanitizedLine).hasMatch()) {
        return;
    }

    appendCurrentInstallLogLine(sanitizedLine);
    appendInstallLogLine(sanitizedLine);

    static const QRegularExpression tracedStepPattern(R"(^\+\s+echo\s+['"]?step:(.*?)['"]?\s*$)");

    QString stepText;
    bool tracedStep = false;
    if (sanitizedLine.startsWith("step:")) {
        stepText = sanitizedLine.mid(5).trimmed();
    } else {
        const QRegularExpressionMatch match = tracedStepPattern.match(sanitizedLine);
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
    const QString targetRoot = targetBuildDirectory();
    const QString hostname = hostnameEdit_->text().trimmed().isEmpty() ? QStringLiteral("lfs")
                                                                       : hostnameEdit_->text().trimmed();
    const QString fqdn = hostname + QStringLiteral(".localdomain");
    const QString username = usernameEdit_->text().trimmed();
    const QString password = passwordEdit_->text();
    const QString timezone = timeZoneCombo_->currentText().trimmed();

    QStringList lines;
    lines << "#!/usr/bin/env bash";
    lines << "";
    lines << "set -euo pipefail";
    lines << "";
    lines << QString("TARGET_ROOT=%1").arg(shellQuote(targetRoot));
    lines << "FILES_DIR=\"${PROJECT_ROOT}/files\"";
    lines << "MOUNTED_POINTS=()";
    lines << "";
    lines << "mount_bind() {";
    lines << "  local source_path=\"$1\"";
    lines << "  local target_path=\"$2\"";
    lines << "  mkdir -p \"$target_path\"";
    lines << "  if mountpoint -q \"$target_path\"; then";
    lines << "    return";
    lines << "  fi";
    lines << "  mount --bind \"$source_path\" \"$target_path\"";
    lines << "  MOUNTED_POINTS=(\"$target_path\" \"${MOUNTED_POINTS[@]}\")";
    lines << "}";
    lines << "";
    lines << "mount_virtual() {";
    lines << "  local fs_type=\"$1\"";
    lines << "  local source_name=\"$2\"";
    lines << "  local target_path=\"$3\"";
    lines << "  mkdir -p \"$target_path\"";
    lines << "  if mountpoint -q \"$target_path\"; then";
    lines << "    return";
    lines << "  fi";
    lines << "  mount -t \"$fs_type\" \"$source_name\" \"$target_path\"";
    lines << "  MOUNTED_POINTS=(\"$target_path\" \"${MOUNTED_POINTS[@]}\")";
    lines << "}";
    lines << "";
    lines << "cleanup() {";
    lines << "  local mount_point";
    lines << "  for mount_point in \"${MOUNTED_POINTS[@]}\"; do";
    lines << "    umount -lf \"$mount_point\" >/dev/null 2>&1 || true";
    lines << "  done";
    lines << "}";
    lines << "trap cleanup EXIT";
    lines << "";
    lines << "echo \"step:Finalizing target system\"";
    lines << "if [ ! -d \"$TARGET_ROOT\" ]; then";
    lines << "  echo \"Target root does not exist: $TARGET_ROOT\" >&2";
    lines << "  exit 1";
    lines << "fi";
    lines << "if [ ! -x \"$TARGET_ROOT/bin/bash\" ]; then";
    lines << "  echo \"Target root is missing /bin/bash: $TARGET_ROOT\" >&2";
    lines << "  exit 1";
    lines << "fi";
    lines << "mount_bind /dev \"$TARGET_ROOT/dev\"";
    lines << "mount_bind /dev/pts \"$TARGET_ROOT/dev/pts\"";
    lines << "mount_virtual proc proc \"$TARGET_ROOT/proc\"";
    lines << "mount_virtual sysfs sysfs \"$TARGET_ROOT/sys\"";
    lines << "mount_virtual tmpfs tmpfs \"$TARGET_ROOT/run\"";
    lines << "install -Dm644 \"$FILES_DIR/hostname\" \"$TARGET_ROOT/etc/hostname\"";
    lines << "install -Dm644 \"$FILES_DIR/fstab\" \"$TARGET_ROOT/etc/fstab\"";
    lines << QString("chroot \"$TARGET_ROOT\" /usr/bin/env -i HOME=/root TERM=\"${TERM:-xterm}\" PATH=/usr/bin:/usr/sbin:/bin:/sbin HOSTNAME_VALUE=%1 FQDN_VALUE=%2 TIMEZONE_VALUE=%3 /bin/bash <<'EOF'")
                 .arg(shellQuote(hostname), shellQuote(fqdn), shellQuote(timezone));
    lines << "set -euo pipefail";
    lines << "if [ -n \"$TIMEZONE_VALUE\" ] && [ -e \"/usr/share/zoneinfo/$TIMEZONE_VALUE\" ]; then";
    lines << "  ln -sf \"/usr/share/zoneinfo/$TIMEZONE_VALUE\" /etc/localtime";
    lines << "fi";
    lines << "cat > /etc/hosts <<EOF_HOSTS";
    lines << "# Begin /etc/hosts";
    lines << "";
    lines << "127.0.0.1 localhost.localdomain localhost";
    lines << "$([ -n \"$HOSTNAME_VALUE\" ] && printf '127.0.1.1 %s %s\n' \"$FQDN_VALUE\" \"$HOSTNAME_VALUE\")";
    lines << "::1       localhost ip6-localhost ip6-loopback";
    lines << "ff02::1   ip6-allnodes";
    lines << "ff02::2   ip6-allrouters";
    lines << "";
    lines << "# End /etc/hosts";
    lines << "EOF_HOSTS";
    lines << QString("if ! id -u %1 >/dev/null 2>&1; then useradd -m %1; fi")
                 .arg(shellQuote(username));
    lines << QString("printf '%s\\n' %1 | chpasswd")
                 .arg(shellQuote(username + ":" + password));
    lines << "EOF";

    return lines.join('\n') + '\n';
}

QString InstallerWindow::buildMlfsDownloadScript(const QString &wgetListPath, const QString &md5ListPath) const
{
    const QString targetRoot = targetBuildDirectory();

    QStringList lines;
    lines << QStringLiteral("#!/usr/bin/env bash");
    lines << QString();
    lines << QStringLiteral("LFS=%1").arg(shellQuote(targetRoot));
    lines << QStringLiteral("export LFS");
    lines << QStringLiteral("echo %1").arg(shellQuote(QStringLiteral("step:Downloading MLFS source packages")));
    lines << QStringLiteral("mkdir -pv \"$LFS/sources\"");
    lines << QStringLiteral("chmod -v a+wt \"$LFS/sources\"");
    lines << QStringLiteral("BUNDLE_URL=%1").arg(shellQuote(QString::fromLatin1(kLfsPackagesArchiveUrl)));
    lines << QStringLiteral("BUNDLE_ARCHIVE=\"$LFS/sources/%1\"").arg(QString::fromLatin1(kLfsPackagesArchiveName));
    lines << QStringLiteral("BUNDLE_EXTRACT_DIR=\"$LFS/sources/.bundle-extract\"");
    lines << QStringLiteral("WGET_LIST=%1").arg(shellQuote(wgetListPath));
    lines << QStringLiteral("MD5_LIST=%1").arg(shellQuote(md5ListPath));
    lines << QStringLiteral("expected_md5_for() {");
    lines << QStringLiteral("    local file_name=\"$1\"");
    lines << QStringLiteral("    local expected_sum listed_name");
    lines << QStringLiteral("    while read -r expected_sum listed_name; do");
    lines << QStringLiteral("        if [ \"$listed_name\" = \"$file_name\" ]; then");
    lines << QStringLiteral("            printf '%s\\n' \"$expected_sum\"");
    lines << QStringLiteral("            return 0");
    lines << QStringLiteral("        fi");
    lines << QStringLiteral("    done < \"$MD5_LIST\"");
    lines << QStringLiteral("    return 1");
    lines << QStringLiteral("}");
    lines << QStringLiteral("file_matches_md5() {");
    lines << QStringLiteral("    local file_name=\"$1\"");
    lines << QStringLiteral("    local expected_sum actual_sum");
    lines << QStringLiteral("    [ -n \"$file_name\" ] || return 1");
    lines << QStringLiteral("    [ -f \"$LFS/sources/$file_name\" ] || return 1");
    lines << QStringLiteral("    expected_sum=$(expected_md5_for \"$file_name\") || return 1");
    lines << QStringLiteral("    actual_sum=$(md5sum \"$LFS/sources/$file_name\")");
    lines << QStringLiteral("    actual_sum=${actual_sum%% *}");
    lines << QStringLiteral("    [ \"$actual_sum\" = \"$expected_sum\" ]");
    lines << QStringLiteral("}");
    lines << QStringLiteral("mirror_candidates() {");
    lines << QStringLiteral("    local url=\"$1\"");
    lines << QStringLiteral("    local file_name=\"$2\"");
    lines << QStringLiteral("    local no_scheme path");
    lines << QStringLiteral("    printf '%s\\n' \"$url\"");
    lines << QStringLiteral("    no_scheme=${url#*://}");
    lines << QStringLiteral("    path=/${no_scheme#*/}");
    lines << QStringLiteral("    case \"$url\" in");
    lines << QStringLiteral("        *://ftp.gnu.org/*|*://alpha.gnu.org/*)");
    lines << QStringLiteral("            printf '%s\\n' \"https://ftpmirror.gnu.org${path}\"");
    lines << QStringLiteral("            if [[ \"$path\" == /gnu/* ]]; then");
    lines << QStringLiteral("                printf '%s\\n' \"https://mirrors.kernel.org/gnu/${path#/gnu/}\"");
    lines << QStringLiteral("            fi");
    lines << QStringLiteral("            ;;");
    lines << QStringLiteral("        *://sourceware.org/*|*://ftp.sourceware.org/*)");
    lines << QStringLiteral("            if [[ \"$path\" == /pub/* ]]; then");
    lines << QStringLiteral("                printf '%s\\n' \"https://mirrors.kernel.org/sourceware/${path#/pub/}\"");
    lines << QStringLiteral("            fi");
    lines << QStringLiteral("            ;;");
    lines << QStringLiteral("        *://www.kernel.org/*|*://cdn.kernel.org/*|*://mirrors.edge.kernel.org/*)");
    lines << QStringLiteral("            printf '%s\\n' \"https://mirrors.edge.kernel.org${path}\"");
    lines << QStringLiteral("            printf '%s\\n' \"https://cdn.kernel.org${path}\"");
    lines << QStringLiteral("            ;;");
    lines << QStringLiteral("    esac");
    lines << QStringLiteral("    if [ -n \"$file_name\" ]; then");
    lines << QStringLiteral("        printf '%s\\n' \"https://fossies.org/linux/misc/${file_name}\"");
    lines << QStringLiteral("        printf '%s\\n' \"https://fossies.org/linux/privat/old/${file_name}\"");
    lines << QStringLiteral("    fi");
    lines << QStringLiteral("}");
    lines << QStringLiteral("download_with_mirrors() {");
    lines << QStringLiteral("    local url=\"$1\"");
    lines << QStringLiteral("    local file_name=\"$2\"");
    lines << QStringLiteral("    local candidate seen_urls");
    lines << QStringLiteral("    seen_urls='|'");
    lines << QStringLiteral("    while IFS= read -r candidate; do");
    lines << QStringLiteral("        [ -n \"$candidate\" ] || continue");
    lines << QStringLiteral("        case \"$seen_urls\" in");
    lines << QStringLiteral("            *\"|$candidate|\"*) continue ;;");
    lines << QStringLiteral("        esac");
    lines << QStringLiteral("        seen_urls+=\"$candidate|\"");
    lines << QStringLiteral("        echo \"Trying source: $candidate\"");
    lines << QStringLiteral("        if wget --continue --directory-prefix=\"$LFS/sources\" --tries=20 --timeout=30 \"$candidate\"; then");
    lines << QStringLiteral("            return 0");
    lines << QStringLiteral("        fi");
    lines << QStringLiteral("    done < <(mirror_candidates \"$url\" \"$file_name\")");
    lines << QStringLiteral("    return 1");
    lines << QStringLiteral("}");
    lines << QStringLiteral("if [ ! -f \"$BUNDLE_ARCHIVE\" ]; then");
    lines << QStringLiteral("    wget -c \"$BUNDLE_URL\" --directory-prefix=\"$LFS/sources\"");
    lines << QStringLiteral("fi");
    lines << QStringLiteral("rm -rf \"$BUNDLE_EXTRACT_DIR\"");
    lines << QStringLiteral("mkdir -p \"$BUNDLE_EXTRACT_DIR\"");
    lines << QStringLiteral("tar -xf \"$BUNDLE_ARCHIVE\" -C \"$BUNDLE_EXTRACT_DIR\"");
    lines << QStringLiteral("bundle_root=$(find \"$BUNDLE_EXTRACT_DIR\" -mindepth 1 -maxdepth 1 -type d | sort | head -n 1)");
    lines << QStringLiteral("if [ -z \"$bundle_root\" ]; then");
    lines << QStringLiteral("    bundle_root=\"$BUNDLE_EXTRACT_DIR\"");
    lines << QStringLiteral("fi");
    lines << QStringLiteral("find \"$bundle_root\" -mindepth 1 -maxdepth 1 -exec cp -an {} \"$LFS/sources/\" \\;");
    lines << QStringLiteral("rm -rf \"$BUNDLE_EXTRACT_DIR\"");
    lines << QStringLiteral("cd \"$LFS/sources\"");
    lines << QStringLiteral("if md5sum -c --status \"$MD5_LIST\"; then");
    lines << QStringLiteral("    echo \"Bundle satisfied package list; skipping individual downloads\"");
    lines << QStringLiteral("    install -m 755 \"$SOURCE_PROJECT_ROOT/tools/autountar.sh\" \"$LFS/sources/autountar\"");
    lines << QStringLiteral("    chown root:root \"$LFS/sources\"/*");
    lines << QStringLiteral("    exit 0");
    lines << QStringLiteral("fi");
    lines << QStringLiteral("mapfile -t download_urls < \"$WGET_LIST\"");
    lines << QStringLiteral("failed_urls=()");
    lines << QStringLiteral("for url in \"${download_urls[@]}\"; do");
    lines << QStringLiteral("    if [ -z \"$url\" ]; then");
    lines << QStringLiteral("        continue");
    lines << QStringLiteral("    fi");
    lines << QStringLiteral("    file_name=\"${url##*/}\"");
    lines << QStringLiteral("    file_name=\"${file_name%%\\?*}\"");
    lines << QStringLiteral("    if file_matches_md5 \"$file_name\"; then");
    lines << QStringLiteral("        continue");
    lines << QStringLiteral("    fi");
    lines << QStringLiteral("    if [ -n \"$file_name\" ] && [ -f \"$LFS/sources/$file_name\" ]; then");
    lines << QStringLiteral("        rm -f \"$LFS/sources/$file_name\"");
    lines << QStringLiteral("    fi");
    lines << QStringLiteral("    if ! download_with_mirrors \"$url\" \"$file_name\"; then");
    lines << QStringLiteral("        echo \"Initial download failed: $url\" >&2");
    lines << QStringLiteral("        failed_urls+=(\"$url\")");
    lines << QStringLiteral("    fi");
    lines << QStringLiteral("done");
    lines << QStringLiteral("if [ \"${#failed_urls[@]}\" -gt 0 ]; then");
    lines << QStringLiteral("    echo \"Retrying ${#failed_urls[@]} failed download(s)...\" >&2");
    lines << QStringLiteral("    retry_urls=()");
    lines << QStringLiteral("    for url in \"${failed_urls[@]}\"; do");
    lines << QStringLiteral("        file_name=\"${url##*/}\"");
    lines << QStringLiteral("        file_name=\"${file_name%%\\?*}\"");
    lines << QStringLiteral("        if file_matches_md5 \"$file_name\"; then");
    lines << QStringLiteral("            continue");
    lines << QStringLiteral("        fi");
    lines << QStringLiteral("        if [ -n \"$file_name\" ] && [ -f \"$LFS/sources/$file_name\" ]; then");
    lines << QStringLiteral("            rm -f \"$LFS/sources/$file_name\"");
    lines << QStringLiteral("        fi");
    lines << QStringLiteral("        sleep 2");
    lines << QStringLiteral("        if ! download_with_mirrors \"$url\" \"$file_name\"; then");
    lines << QStringLiteral("            echo \"Download still failing: $url\" >&2");
    lines << QStringLiteral("            retry_urls+=(\"$url\")");
    lines << QStringLiteral("        fi");
    lines << QStringLiteral("    done");
    lines << QStringLiteral("    failed_urls=(\"${retry_urls[@]}\")");
    lines << QStringLiteral("fi");
    lines << QStringLiteral("if [ \"${#failed_urls[@]}\" -gt 0 ]; then");
    lines << QStringLiteral("    printf '%s\\n' 'Unrecoverable download failures:' >&2");
    lines << QStringLiteral("    printf '  %s\\n' \"${failed_urls[@]}\" >&2");
    lines << QStringLiteral("    exit 1");
    lines << QStringLiteral("fi");
    lines << QStringLiteral("md5sum -c \"$MD5_LIST\"");
    lines << QStringLiteral("install -m 755 \"$SOURCE_PROJECT_ROOT/tools/autountar.sh\" \"$LFS/sources/autountar\"");
    lines << QStringLiteral("chown root:root \"$LFS/sources\"/*");

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
    return timeZoneCombo_->currentText().trimmed() + '\n';
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

QString InstallerWindow::targetBuildDirectory() const
{
    const QVector<PlannedPartition> partitions = collectPartitions();
    for (const PlannedPartition &partition : partitions) {
        if (partition.mountPoint == "/" && !partition.localMountPoint.trimmed().isEmpty()) {
            return partition.localMountPoint.trimmed();
        }
    }

    return QStringLiteral("/mnt/lfs");
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
