#include "infrastructure/update/updatechecker.h"

#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStringView>
#include <QVersionNumber>

#include "application/applicationconstants.h"
#include "infrastructure/coreprocess/coreexecutable.h"

namespace
{
enum class VersionSuffixKind
{
    Prerelease,
    Stable,
    Hotfix
};

QString normalizedSuffix(const QString &version, qsizetype suffixIndex)
{
    QString suffix = version.mid(suffixIndex);
    while (!suffix.isEmpty() && QStringLiteral("-+_.").contains(suffix.front()))
    {
        suffix.removeFirst();
    }
    return suffix;
}

VersionSuffixKind suffixKind(const QString &suffix)
{
    if (suffix.isEmpty())
    {
        return VersionSuffixKind::Stable;
    }
    if (suffix.startsWith(QStringLiteral("hotfix"), Qt::CaseInsensitive))
    {
        return VersionSuffixKind::Hotfix;
    }
    return VersionSuffixKind::Prerelease;
}

int compareSuffixes(const QString &currentSuffix, const QString &latestSuffix)
{
    qsizetype currentIndex = 0;
    qsizetype latestIndex = 0;
    while (currentIndex < currentSuffix.size() &&
           latestIndex < latestSuffix.size())
    {
        const bool currentIsDigit = currentSuffix.at(currentIndex).isDigit();
        const bool latestIsDigit = latestSuffix.at(latestIndex).isDigit();
        if (currentIsDigit && latestIsDigit)
        {
            qsizetype currentEnd = currentIndex;
            qsizetype latestEnd = latestIndex;
            while (currentEnd < currentSuffix.size() &&
                   currentSuffix.at(currentEnd).isDigit())
            {
                ++currentEnd;
            }
            while (latestEnd < latestSuffix.size() &&
                   latestSuffix.at(latestEnd).isDigit())
            {
                ++latestEnd;
            }

            qsizetype currentNumberStart = currentIndex;
            qsizetype latestNumberStart = latestIndex;
            while (currentNumberStart < currentEnd - 1 &&
                   currentSuffix.at(currentNumberStart) == '0')
            {
                ++currentNumberStart;
            }
            while (latestNumberStart < latestEnd - 1 &&
                   latestSuffix.at(latestNumberStart) == '0')
            {
                ++latestNumberStart;
            }

            const qsizetype currentLength = currentEnd - currentNumberStart;
            const qsizetype latestLength = latestEnd - latestNumberStart;
            if (currentLength != latestLength)
            {
                return currentLength < latestLength ? -1 : 1;
            }

            const QStringView currentNumber(currentSuffix);
            const QStringView latestNumber(latestSuffix);
            const int numberComparison =
                currentNumber.mid(currentNumberStart, currentLength).compare(
                    latestNumber.mid(latestNumberStart, latestLength)
                );
            if (numberComparison != 0)
            {
                return numberComparison;
            }

            currentIndex = currentEnd;
            latestIndex = latestEnd;
            continue;
        }

        const QChar currentCharacter = currentSuffix.at(currentIndex).toLower();
        const QChar latestCharacter = latestSuffix.at(latestIndex).toLower();
        if (currentCharacter != latestCharacter)
        {
            return currentCharacter < latestCharacter ? -1 : 1;
        }
        ++currentIndex;
        ++latestIndex;
    }

    if (currentIndex == currentSuffix.size() &&
        latestIndex == latestSuffix.size())
    {
        return 0;
    }
    return currentIndex == currentSuffix.size() ? -1 : 1;
}
}

UpdateChecker::UpdateChecker(QObject *parent)
    : QObject(parent),
      versions{
          QApplication::applicationVersion(),
          QStringLiteral("正在检查"),
          QStringLiteral("未知"),
          QStringLiteral("正在检查")
      },
      uiNetworkManager(new QNetworkAccessManager(this)),
      coreNetworkManager(new QNetworkAccessManager(this))
{
    connect(uiNetworkManager, &QNetworkAccessManager::finished,
            this, &UpdateChecker::handleUiReply);
    connect(coreNetworkManager, &QNetworkAccessManager::finished,
            this, &UpdateChecker::handleCoreReply);
}

const VersionInfo &UpdateChecker::versionInfo() const
{
    return versions;
}

void UpdateChecker::markDisabled()
{
    versions.uiLatest = QStringLiteral("已禁用");
    versions.coreLatest = QStringLiteral("已禁用");
    emit versionInfoChanged(versions);
}

void UpdateChecker::check()
{
    try
    {
        versions.coreVersion = CoreExecutable::version(this);
        qInfo().noquote() << "检查核心版本成功：" + versions.coreVersion;
    }
    catch (const std::runtime_error &error)
    {
        qWarning().noquote() << "检查核心版本失败：" + QString(error.what());
        versions.coreVersion = QStringLiteral("错误");
    }
    emit versionInfoChanged(versions);

    uiNetworkManager->get(QNetworkRequest(
        QUrl(
            "https://api.github.com/repos/" +
            ApplicationConstants::RepositoryName +
            "/releases/latest"
        )
    ));
    coreNetworkManager->get(QNetworkRequest(
        QUrl(
            "https://api.github.com/repos/" +
            ApplicationConstants::CoreRepositoryName +
            "/commits/" +
            ApplicationConstants::CoreBranchName
        )
    ));
}

QString UpdateChecker::normalizeVersionTag(const QString &tag)
{
    return tag.startsWith('v') ? tag.mid(1) : tag;
}

std::optional<QString> UpdateChecker::parseReleaseVersion(
    const QByteArray &payload
)
{
    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        return std::nullopt;
    }

    const QJsonValue tagValue = document.object().value("tag_name");
    if (!tagValue.isString())
    {
        return std::nullopt;
    }

    const QString version =
        normalizeVersionTag(tagValue.toString().trimmed());
    return version.isEmpty()
        ? std::nullopt
        : std::optional<QString>(version);
}

std::optional<QString> UpdateChecker::parseBranchHeadCommit(
    const QByteArray &payload
)
{
    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        return std::nullopt;
    }

    const QJsonValue shaValue = document.object().value("sha");
    if (!shaValue.isString())
    {
        return std::nullopt;
    }

    const QString sha = shaValue.toString().trimmed().toLower();
    if (sha.size() < 7)
    {
        return std::nullopt;
    }
    for (const QChar character : sha)
    {
        if (!QStringLiteral("0123456789abcdef").contains(character))
        {
            return std::nullopt;
        }
    }
    return sha;
}

QString UpdateChecker::coreCommitFromVersion(const QString &coreVersion)
{
    // Bundled core version format: "<branch>-<short commit>", e.g. "travelsky-8060805"
    const qsizetype separator = coreVersion.lastIndexOf('-');
    if (separator < 0)
    {
        return {};
    }
    const QString commit = coreVersion.mid(separator + 1).trimmed().toLower();
    if (commit.size() < 7)
    {
        return {};
    }
    for (const QChar character : commit)
    {
        if (!QStringLiteral("0123456789abcdef").contains(character))
        {
            return {};
        }
    }
    return commit;
}

bool UpdateChecker::isCoreOutdated(
    const QString &coreVersion,
    const QString &latestCommit
)
{
    const QString currentCommit = coreCommitFromVersion(coreVersion);
    if (currentCommit.isEmpty())
    {
        return true;
    }
    return !latestCommit.toLower().startsWith(currentCommit);
}

bool UpdateChecker::isNewerVersion(const QString &currentVersion, const QString &latestVersion)
{
    qsizetype currentSuffix = 0;
    qsizetype latestSuffix = 0;
    const QVersionNumber current = QVersionNumber::fromString(currentVersion, &currentSuffix);
    const QVersionNumber latest = QVersionNumber::fromString(latestVersion, &latestSuffix);

    if (latest != current)
    {
        return latest > current;
    }

    const QString currentSuffixText =
        normalizedSuffix(currentVersion, currentSuffix);
    const QString latestSuffixText =
        normalizedSuffix(latestVersion, latestSuffix);
    const VersionSuffixKind currentKind = suffixKind(currentSuffixText);
    const VersionSuffixKind latestKind = suffixKind(latestSuffixText);
    if (latestKind != currentKind)
    {
        return latestKind > currentKind;
    }
    return compareSuffixes(currentSuffixText, latestSuffixText) < 0;
}

void UpdateChecker::handleUiReply(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError)
    {
        const QString reason = reply->errorString();
        qWarning().noquote() << "检查 UI 更新失败。原因是：" + reason;
        versions.uiLatest = QStringLiteral("检查失败");
        reply->deleteLater();
        emit checkFailed(UpdateComponent::Ui, reason);
        emit versionInfoChanged(versions);
        return;
    }

    const std::optional<QString> latestVersion =
        parseReleaseVersion(reply->readAll());
    reply->deleteLater();
    if (!latestVersion.has_value())
    {
        const QString reason =
            QStringLiteral("响应中缺少有效的版本标签");
        qWarning().noquote() << "检查 UI 更新失败。原因是：" + reason;
        versions.uiLatest = QStringLiteral("检查失败");
        emit checkFailed(UpdateComponent::Ui, reason);
        emit versionInfoChanged(versions);
        return;
    }

    qInfo().noquote()
        << "检查 UI 更新成功。最新版本：" + *latestVersion;
    versions.uiLatest = *latestVersion;
    emit versionInfoChanged(versions);

    if (isNewerVersion(versions.uiVersion, *latestVersion))
    {
        emit uiUpdateAvailable(*latestVersion);
    }
}

void UpdateChecker::handleCoreReply(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError)
    {
        const QString reason = reply->errorString();
        qWarning().noquote() << "检查核心更新失败。原因是：" + reason;
        versions.coreLatest = QStringLiteral("检查失败");
        reply->deleteLater();
        emit checkFailed(UpdateComponent::Core, reason);
        emit versionInfoChanged(versions);
        return;
    }

    const std::optional<QString> latestCommit =
        parseBranchHeadCommit(reply->readAll());
    reply->deleteLater();
    if (!latestCommit.has_value())
    {
        const QString reason =
            QStringLiteral("响应中缺少有效的提交哈希");
        qWarning().noquote() << "检查核心更新失败。原因是：" + reason;
        versions.coreLatest = QStringLiteral("检查失败");
        emit checkFailed(UpdateComponent::Core, reason);
        emit versionInfoChanged(versions);
        return;
    }

    versions.coreLatest =
        ApplicationConstants::CoreBranchName + "-" + latestCommit->left(7);
    qInfo().noquote()
        << "检查核心更新成功。最新版本：" + versions.coreLatest;
    emit versionInfoChanged(versions);

    if (isCoreOutdated(versions.coreVersion, *latestCommit))
    {
        qInfo().noquote() << "核心版本存在更新，可手动更新或通知开发者更新。";
    }
}
