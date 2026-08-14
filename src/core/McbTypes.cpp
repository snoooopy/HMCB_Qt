#include "McbTypes.h"
#include <QMap>

namespace Mcb {

StatusParam decodeStatusParam(LPARAM lParam) {
    uint32_t v = static_cast<uint32_t>(lParam);
    return { static_cast<int>((v >> 16) & 0xFFFF), v & 0xFFFF };
}

QString statusCategoryStr(WPARAM c) {
    switch (c) {
    case MCB_DVSTS:    return QStringLiteral("DVSTS");
    case MCB_DVREADY:  return QStringLiteral("DVREADY");
    case MCB_DVERR:    return QStringLiteral("DVERR");
    case MCB_STARTERR: return QStringLiteral("STARTERR");
    case MCB_STOPERR:  return QStringLiteral("STOPERR");
    case MCB_STOPOK:   return QStringLiteral("STOPOK");
    default:           return QStringLiteral("CAT_%1").arg(static_cast<qulonglong>(c), 4, 16, QChar('0'));
    }
}

QString statusCodeStr(uint32_t code) {
    static const QMap<uint32_t, QString> m = {
        {0x0101, "CONNECTOK"},   {0x0102, "CONNECTCLOSE"}, {0x0103, "CTRLCONNECTOK"},
        {0x0110, "CONNECTFAIL"}, {0x0111, "SOCKETFAIL"},
        {0x0200, "MBFPGACFG_OK"},{0x0201, "ADFPGACFG_OK"}, {0x0204, "REGSET_OK"},
        {0x0210, "FPGAFILE_ERR"},{0x0211, "MBFPGACFG_ERR"},{0x0214, "REGSET_ERR"},
        {0x0300, "STARTOK"},     {0x1123, "SETNEWIP_OK"},  {0x1122, "SETNEWIP_ERR"},
        {0x1303, "NODATA"},      {0x1400, "SCNTPCNTDIFF"}
    };
    return m.value(code, QStringLiteral("CODE_%1").arg(code, 4, 16, QChar('0')));
}

} // namespace Mcb
