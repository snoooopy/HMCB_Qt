# 多版本 Ubuntu 打包兼容方案（20.04 → 24.04+）

**目标**：一份 dist 便携包（tar.gz 解压即用，无需 apt 安装任何依赖），在
Ubuntu 20.04 / 22.04 / 24.04 及更新的 x86-64 系统上均可运行。

**产出**：`dist/HMCB_Qt_<日期>.tar.gz`（约 26 MB）。
一键打包：`tools/pack_compat/make_compat_package.sh`（需 Docker）。

---

## 1. 原理：为什么旧包在 20.04/22.04 上起不来

Linux 程序的 ELF 头里记录着它需要的 glibc 符号版本（如 `GLIBC_2.38`）。
系统 glibc 只向下兼容：新系统跑老程序没问题，**老系统没有新符号**，加载器
直接报 `version GLIBC_2.38 not found`，程序起不来。

旧包（24.04 上构建）的判决数据：

| 产物 | 需要的符号 |
|---|---|
| bin/HMCB_Qt | GLIBC_2.34 / GLIBCXX_3.4.29 |
| libQt6Core.so.6 | GLIBC_2.35 / GLIBCXX_3.4.30 |
| libicu74 / libX11 / libxcb / libharfbuzz 等约 20 个库 | GLIBC_2.38 |
| libmcbctrl.so.1.0.0.131（v6） | GLIBC_2.38 / GLIBCXX_3.4.32 |

而各版本系统 glibc 上限：**20.04 = 2.31，22.04 = 2.35，24.04 = 2.39**。
glibc 本身不可随包分发（它必须与系统内核/动态加载器配对），libstdc++/libgcc
自带也有雷。唯一稳妥思路：**在最低目标版本（20.04）环境里构建一切**，符号
需求天然 ≤ 2.31，向上兼容所有新系统。

## 2. 方案：20.04 容器 + Qt 官方二进制 + 依赖自带

```
宿主机 (任意版本, 需 Docker)
  └─ tools/pack_compat/make_compat_package.sh
       ├─ fetch_qt_base.sh        宿主下载 Qt 6.4.3 官方归档并校验
       │    （容器内下载会截断损坏，见 §4 坑①）
       ├─ docker build            镜像 hmcb-qt-build:focal
       │    ubuntu:20.04 + g++9 + cmake + Qt 6.4.3 官方二进制(RHEL8 构建,
       │    glibc 需求仅 2.28) + icu56 + 20.04 全套 GUI 运行库(依赖收集源)
       └─ docker run              容器内 build_in_container.sh：
            1. g++9 重编 libmcbctrl v6（-Wl,-soname + -lstdc++fs）
            2. CMake 构建 HMCB_Qt（Qt 6.4.3）+ ctest（14/14）
            3. make_package.sh 递归 ldd 收集 20.04 依赖 → dist 包
            4. audit_symbols.sh 全包符号断言（≤2.31 / ≤3.4.28）
            5. tar.gz 落宿主 dist/
```

包内 39 个 ELF 审计结果：**全部 GLIBC ≤ 2.31、GLIBCXX ≤ 3.4.28**。
系统级依赖只剩 glibc/libstdc++/libgcc（20.04+ 均满足），其余全在包里
（lib/ 目录 + run.sh 设置 LD_LIBRARY_PATH）。

## 3. 验证矩阵（2026-09-21，包 HMCB_Qt_20260921）

| 环境 | 方式 | 结果 |
|---|---|---|
| Ubuntu 20.04（干净容器，仅 glibc） | offscreen 运行 20s | ✅ 存活，无符号缺失 |
| Ubuntu 22.04（干净容器，仅 glibc） | offscreen 运行 20s | ✅ 存活，无符号缺失 |
| Ubuntu 24.04（本机，真实环境） | offscreen 运行 20s | ✅ 存活 |

复验命令：`tools/pack_compat/smoke_compat.sh [dist/HMCB_Qt_<日期>]`
（自动解包目录版后跑三环境；干净容器验证"包自带一切，仅依赖系统 glibc"。）

## 4. 踩坑记录（容器/网络/工具链）

1. **容器内下载必损坏**：aqtinstall/curl 在容器内下载 Qt 7z 一律校验失败
   （tuna/USTC 皆然），宿主机同 URL 完好 → 解法：宿主 `fetch_qt_base.sh`
   下载校验后 `COPY` 进镜像。qtbase 用官方 SHA256；icu 归档 Updates.xml 无
   SHA，用「官方 .sha1 双镜像比对 + sha1sum 校验」三重验证。
2. **Docker Hub 被墙**：`docker.m.daocloud.io` 代理拉取后 retag。
3. **qtbase 归档不含 icu**：Qt 官方二进制链接 icu 56，独立归档
   `icu-linux-Rhel7.2-x64.7z`，解压到同一 `/opt/Qt`。
4. **7z 解压目录 700 权限**：`/opt/Qt` 解出来是 `drwx------ root`，构建
   容器以非 root 跑时 find_package 静默失败 → 解压后 `chmod -R a+rX`。
5. **cmake 3.16 无 `ctest --test-dir`**：用 `(cd build && ctest)`。
6. **GCC9 的 std::filesystem** 需显式 `-lstdc++fs`。
7. **cmake ≤3.19 对 custom_target 里 `$<TARGET_FILE_DIR:...>` 建隐式依赖**，
   与 `add_dependencies` 成环（Generate 失败）→ src/CMakeLists.txt 的
   sync_config 改用 `${CMAKE_BINARY_DIR}` 直写（见该处注释）。
8. **cmake3.16/gcc9 链无 SONAME 的 .so 时 NEEDED 记相对路径**，ctest 从
   build/tests 跑即断链 → 容器内编 libmcbctrl 加 `-Wl,-soname`（与宿主
   cmake3.28/gcc13 产物行为一致）。
9. **`docker run` 不加 `-i` 时 heredoc 不进容器**，`bash -s` 读 EOF 秒退 0
   ——冒烟假阳性，务必 `-i`。
10. **apt 层包名**：focal 是 `libxcb-icccm4`（不是 …1）；Qt6 Widgets 的
    find_package 需要 OpenGL 开发包（`libgl-dev libopengl-dev libglx-dev`），
    只装运行库不够。
11. **`docker build | tail` 会吞退出码**：日志重定向后 `echo EXIT=$?`。

## 5. 日常使用

```bash
# 出包（宿主机任意 Ubuntu，需 docker + 能访问清华/USTC 镜像）
tools/pack_compat/make_compat_package.sh            # 增量
tools/pack_compat/make_compat_package.sh --no-cache # 全新构建

# 复验（可选）
cd dist && tar xzf HMCB_Qt_<日期>.tar.gz            # smoke 需目录版
tools/pack_compat/smoke_compat.sh dist/HMCB_Qt_<日期>

# 单独审计任一包的符号
tools/audit_symbols.sh dist/HMCB_Qt_<日期> 2.31 3.4.28
```

目标机部署：解压 → `./run.sh`。无需安装任何依赖（20.04+ x86-64）。

## 6. 维护提示

- **升级 Qt / libmcbctrl 后重新出包**即可，流水线幂等；符号审计会自动
  把关（任何产物需求 > 2.31/3.4.28 直接 FAIL）。
- 若未来要支持比 20.04 更老的系统，需换更老的基础镜像重建（glibc 需求
  由基础镜像决定）；支持更新的 Ubuntu 无需任何动作（向上兼容）。
- 打包脚本产物不进 git（`.gitignore` 已含 `dist/`、`qtbase.7z`、`icu.7z`）。
