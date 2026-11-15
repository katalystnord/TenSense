#include "core/ApplicationInfo.h"

namespace ts::core {

QString applicationName()
{
    return QStringLiteral("TenSense");
}

QString applicationSubtitle()
{
    return QStringLiteral("open universal testing software");
}

QString applicationVersion()
{
    return QStringLiteral(TS_VERSION);
}

QString applicationBuild()
{
    return QStringLiteral(TS_BUILD);
}

} // namespace ts::core
