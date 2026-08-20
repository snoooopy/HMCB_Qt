#pragma once
#include <QString>

// 运行期 config 目录解析（两种部署布局统一）：
//   构建树：exe 与 config 同目录（CMake sync_config / Qt Creator 构建拷贝）
//   dist 包：bin/HMCB_Qt + 包根 config/（bin 的上一级）
// 程序原先只认 <exe>/config——dist 布局下读不到 app.ini，静默落回 mock
// 缺省（改包根 app.ini 的 Mode=real 不生效），故按 优先 exe/config、
// 回退 exe/../config 解析。
class AppPaths {
public:
    // 无参：按当前可执行文件解析
    static QString configDir();
    // 纯函数（供单测注入 exe 目录）
    static QString configDirFor(const QString& exeDir);
};
