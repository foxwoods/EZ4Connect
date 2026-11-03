#include <QCoreApplication>
#include <QDebug>

#include "infrastructure/update/updatechecker.h"

namespace
{
bool normalizesTags()
{
    const bool passed =
        UpdateChecker::normalizeVersionTag("v1.2.3") == "1.2.3"
        && UpdateChecker::normalizeVersionTag("1.2.3") == "1.2.3";
    if (!passed)
    {
        qCritical() << "normalizesTags failed";
    }
    return passed;
}

bool validatesReleaseResponses()
{
    const std::optional<QString> version =
        UpdateChecker::parseReleaseVersion(R"({"tag_name":"v1.2.3"})");
    const bool passed =
        version == std::optional<QString>("1.2.3")
        && !UpdateChecker::parseReleaseVersion("not json").has_value()
        && !UpdateChecker::parseReleaseVersion("{}").has_value()
        && !UpdateChecker::parseReleaseVersion(
            R"({"tag_name":""})"
        ).has_value()
        && !UpdateChecker::parseReleaseVersion(
            R"({"tag_name":42})"
        ).has_value();
    if (!passed)
    {
        qCritical() << "validatesReleaseResponses failed";
    }
    return passed;
}

bool comparesVersions()
{
    struct VersionComparison
    {
        QString current;
        QString latest;
        bool expected;
    };
    const QList<VersionComparison> comparisons{
        {"1.2.3", "1.2.4", true},
        {"1.2.3-beta", "1.2.3", true},
        {"1.2.3-pre.2", "1.2.3-pre.10", true},
        {"1.2.3", "1.2.3-hotfix", true},
        {"1.2.3-hotfix.2", "1.2.3-hotfix.10", true},
        {"1.2.3", "1.2.3", false},
        {"1.2.3", "1.2.3-beta", false},
        {"1.2.3-hotfix", "1.2.3", false},
        {"1.3.0", "1.2.9", false}
    };

    for (const VersionComparison &comparison : comparisons)
    {
        const bool actual = UpdateChecker::isNewerVersion(
            comparison.current,
            comparison.latest
        );
        if (actual != comparison.expected)
        {
            qCritical()
                << "comparesVersions failed:"
                << comparison.current
                << comparison.latest
                << "expected" << comparison.expected
                << "actual" << actual;
            return false;
        }
    }
    return true;
}

bool checksCoreBranchHead()
{
    const QString head = "8060805aabbccddeeff00112233445566778899a";
    const bool passed =
        UpdateChecker::parseBranchHeadCommit(
            QByteArray(R"({"sha":")") + head.toUtf8() + R"("})"
        ) == std::optional<QString>(head)
        && !UpdateChecker::parseBranchHeadCommit("not json").has_value()
        && !UpdateChecker::parseBranchHeadCommit("{}").has_value()
        && !UpdateChecker::parseBranchHeadCommit(R"({"sha":"xyz"})").has_value()
        && UpdateChecker::coreCommitFromVersion("travelsky-8060805") == "8060805"
        && UpdateChecker::coreCommitFromVersion("0.9.0").isEmpty()
        && !UpdateChecker::isCoreOutdated("travelsky-8060805", head)
        && UpdateChecker::isCoreOutdated("travelsky-1234567", head)
        && UpdateChecker::isCoreOutdated("0.9.0", head)
        && UpdateChecker::isCoreOutdated("错误", head);
    if (!passed)
    {
        qCritical() << "checksCoreBranchHead failed";
    }
    return passed;
}
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    return normalizesTags()
        && validatesReleaseResponses()
        && comparesVersions()
        && checksCoreBranchHead() ? 0 : 1;
}
