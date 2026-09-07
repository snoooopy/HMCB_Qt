# HMCB 长时间断电后无法重连——根因与修复方案

> 现象：程序连接设备成功后设备断电，短时间断电重连成功；断电一晚上后设备上电**永远无法重连**。Windows 版与 Linux 版库均存在。
> 库版本：libmcbctrl.so.1.0.0.131（Ver-1.3.3，libMcbctrl_HMCB_20260421）
> 状态：**仅方案，未修改任何源文件**

---

## 1. 根因结论

**库的自动重连逻辑在设备断电期间每 5 秒重试一次连接，`ETH_init()` 在 connect 失败路径直接 `return -1`，不关闭、不 delete 刚创建的 socket——每次重试泄漏 1 个文件描述符（fd）。断电一晚上 ≈ 5000+ 次重试 ≈ 泄漏 5000+ fd，超过进程默认上限 `ulimit -n = 1024`（约 85 分钟耗尽），此后所有 socket 创建永远返回 EMFILE，设备上不上电都连不上。**

短时间断电只泄漏几十~几百 fd，未到上限，所以能重连。`ETH_init` 的 boost::asio 分支是平台无关代码（旧 BSD socket 实现已整段注释），Windows 版走同一段，同样泄漏——这解释了两个平台症状一致。

### 证据链

| 环节 | 位置 | 行为 |
|---|---|---|
| 定时线程 | `MCBCtrl.cpp:338 MCBTimer()` | 每 200ms 调一次 `OnTimer()` |
| 断链探测 | `MCBCtrlC.cpp:4008 CheckConnect()` | 5s 无活动 → 读寄存器失败 → 直接调 `InitialMCB_HMCB4()` |
| 兜底重试 | `MCBCtrlC.cpp:4111 CheckDVReadySts()` | AUTOCONNECT=1（默认）时 10s 节流再重试初始化 |
| **泄漏点** | `ETH_func.cpp:128-132 ETH_init()` | connect 失败 `return -1`，`new` 出的 socket 不 close 不 delete |
| 不触及 | `ETH_func.cpp:134` | 失败路径不 `push_back`，后续清理逻辑永远碰不到泄漏对象 |

验证（已具备条件随时可测）：
```bash
pid=$(pgrep HMCB_Qt)
ls /proc/$pid/fd | wc -l            # 断电期间每 ~5s +1；过夜后 ≈1024 顶满
cat /proc/$pid/limits | grep files  # "open files" 1024
# 重启上位机程序 fd 清零 → 应能立即连上（可反证根因）
```

---

## 2. 修改总览

| # | 文件:行号 | 函数 | 性质 | 目的 |
|---|---|---|---|---|
| 1 | `src/ETH_func.cpp:128-132` | `ETH_init()` | **必修（根因）** | connect 失败路径释放 socket，止住 fd 泄漏 |
| 2 | `src/ETH_func.cpp:196-215` | `ETH_CTRL_close()` | **必修（次生）** | 去掉"看起来健康就不清理"的条件分支，改为无条件释放并槽位置空 |
| 3 | `src/ETH_func.cpp:134` | `ETH_init()` | **必修（配套）** | 成功路径 push_back 改为按 nMCB 槽位放置，消除 erase+push_back 索引错位 |
| 4 | `src/ETH_func.cpp:245` 附近 | `ETH_CTRL_write()` | 可选加固 | 入口加越界/空指针防御（原库 vector 空时 `operator[]` 越界为 UB，疑似即崩溃点 0x60D42） |
| 5 | `src/ETH_func.cpp:111` | `ETH_init()` | 可选加固 | connect 加 3s 超时，避免跨网段黑洞场景阻塞 ~127s + 线程堆积 |

> 所有文件指 `/home/jinnxf/workplace_jinn/HMCB_Demo/libMcbctrl_HMCB_20260421/src/`。
> 源文件为 ISO-8859/CRLF 编码：查看/编辑须注意，grep 须加 `-a`。

---

## 3. 具体修改内容

### 修改 1（根因）：`ETH_init()` connect 失败路径释放 socket

**文件**：`src/ETH_func.cpp`　**行号**：128-132

**修改前**（现状原文）：

```cpp
128:    if(ec)
129:    {
130:        std::cout<<boost::system::system_error(ec).what()<< std::endl;
131:        return -1;
132:    }
```

**修改后**：

```cpp
    if(ec)
    {
        std::cout<<boost::system::system_error(ec).what()<< std::endl;
        //【修复】connect 失败后 fd 仍处于打开状态，必须显式释放。
        //否则自动重连每 5 秒泄漏 1 个 fd，断电约 85 分钟后 fd 耗尽(EMFILE)，
        //设备再次上电也无法重连。
        boost::system::error_code ignore_ec;
        sket->close(ignore_ec);
        delete sket;
        return -1;
    }
```

**说明**：boost::asio 的 `tcp::socket` 在 `connect()` 失败后 fd 并不关闭（可重试连接），只有 `close()` 或析构才释放；原代码 `new` 之后失败返回，对象与 fd 双泄漏。`close(ignore_ec)` 用 error_code 重载避免二次异常。此一处即根治 fd 泄漏。

---

### 修改 2（次生）：`ETH_CTRL_close()` 无条件清理 + 槽位置空

**文件**：`src/ETH_func.cpp`　**行号**：196-215（函数体 Linux 分支；`#if` Windows 分支 193-194 不动）

**修改前**（现状原文）：

```cpp
196:    if(_ETH_CTRL_socket.size()<=0)
197:        return;
198:    if(nMCB>=_ETH_CTRL_socket.size())
199:        return;
200:    auto fd_mcb = _ETH_CTRL_socket[nMCB];
201:    struct tcp_info info;
202:    int leninfo = sizeof(info);
203:    getsockopt(fd_mcb->native_handle(), IPPROTO_TCP, TCP_INFO, &info, (socklen_t *)&leninfo);
204:
205:    if (info.tcpi_state == TCP_ESTABLISHED && info.tcpi_lost==0 )
206:    {
207:
208:
209:    }
210:    else
211:    {
212:         fd_mcb->close();
213:         _ETH_CTRL_socket.erase(_ETH_CTRL_socket.begin()+nMCB,_ETH_CTRL_socket.begin()+nMCB+1);
214:         delete fd_mcb;
215:    }
```

**修改后**：

```cpp
    if(_ETH_CTRL_socket.size()<=0)
        return;
    if(nMCB>=_ETH_CTRL_socket.size())
        return;
    //【修复】去掉 TCP_INFO 条件判断：断电后无未确认数据的旧 socket 在本地
    //看来仍是 ESTABLISHED/lost==0，条件分支导致它永远不被清理，重连后
    //_ETH_CTRL_socket[0] 仍指向死 socket（新连接建好了也用不上）。
    auto fd_mcb = _ETH_CTRL_socket[nMCB];
    if (fd_mcb)
    {
        boost::system::error_code ignore_ec;
        fd_mcb->close(ignore_ec);
        delete fd_mcb;
    }
    //【修复】不再 erase（erase 会移动后续元素，多 MCB 时索引错位），
    //改为槽位置空；ETH_CTRL_IsValid() 对 nullptr 返回 false，语义不变。
    _ETH_CTRL_socket[nMCB] = nullptr;
```

**说明**：原条件分支（205-209 空语句）让"无未确认数据"的死 socket 滞留 vector；`erase` 配合 `ETH_init` 的 `push_back` 在重连场景产生索引错位。改为"无条件释放 + 槽位置空"后语义清晰：槽位有值 ⇔ 该 MCB 有活连接。

---

### 修改 3（配套）：`ETH_init()` 成功路径按槽位放置

**文件**：`src/ETH_func.cpp`　**行号**：134

**修改前**（现状原文）：

```cpp
134:    _ETH_CTRL_socket.push_back(sket);
```

**修改后**：

```cpp
    //【修复】按 nMCB 槽位放置（push_back 追加到尾部，与 erase/槽位语义不匹配）；
    //若槽位有残留旧指针（异常路径）先释放，防止泄漏。
    if (_ETH_CTRL_socket.size() <= nMCB)
        _ETH_CTRL_socket.resize(nMCB + 1, nullptr);
    if (_ETH_CTRL_socket[nMCB])
    {
        boost::system::error_code ignore_ec;
        _ETH_CTRL_socket[nMCB]->close(ignore_ec);
        delete _ETH_CTRL_socket[nMCB];
    }
    _ETH_CTRL_socket[nMCB] = sket;
```

**说明**：与修改 2 配套，`vector<socket*>` 从"动态追加/删除"改为"按 MCB 号固定槽位"。单 MCB（实机 MCBNUM=1）下与原行为等价；多 MCB 重连时修复了索引错位。`ETH_CTRL_IsValid()`（ETH_func.cpp:687）已对越界/空指针/is_open 三重检查，置空后返回 false，无需改动。

---

### 修改 4（可选加固）：`ETH_CTRL_write()` 入口防御

**文件**：`src/ETH_func.cpp`　**行号**：245（`ETH_CTRL_write` 内首个 `getsockopt` 之前）

**现状**：`ETH_CTRL_write()` 第 245 行直接 `_ETH_CTRL_socket[nMCB]->native_handle()`，vector 为空/槽位为空时 `operator[]` 越界是 UB（大概率即此前观察到的 libmcbctrl 偏移 0x60D42 段错误）。

**修改内容**（在函数开头、`_rbcp_packet_ID++;` 之后插入）：

```cpp
    //【加固】未连接时直接失败返回，避免越界/空指针 UB
    if (nMCB >= _ETH_CTRL_socket.size() || !_ETH_CTRL_socket[nMCB]
        || !_ETH_CTRL_socket[nMCB]->is_open())
        return -1;
```

`ETH_CTRL_read` 及 `ETH_CTRL_read_buffer_residual_data` 存在同样裸访问，可按同样方式加（文档不逐个贴行号，实施时以 `grep -an "_ETH_CTRL_socket\[nMCB\]" ETH_func.cpp` 清单为准）。

---

### 修改 5（可选加固）：connect 超时

**文件**：`src/ETH_func.cpp`　**行号**：111

**现状**：`sket->connect(ep,ec)` 为同步阻塞、无超时。同网段设备断电时 ARP 探测约 3s 后返回 EHOSTUNREACH（影响小）；但目标经路由/跨网段黑洞时内核 SYN 重传要 ~127s，期间 `MCBTimer` 每 200ms 再起一个 detached 线程跑 `OnTimer`（MCBCtrl.cpp:362），线程堆积。

**修改内容**（替换第 111 行）：

```cpp
    //【加固】3 秒超时 connect，替代无限期阻塞
    sket->async_connect(ep, [&](const boost::system::error_code& e){ ec = e; });
    if (iosrv.run_for(std::chrono::seconds(3)) == 0)   // 0 = 超时，无完成处理器
    {
        boost::system::error_code ignore_ec;
        sket->close(ignore_ec);
        delete sket;
        return -1;
    }
```

需 `#include <chrono>`（asio 头已连带）。此项不改也不影响根因修复，建议实机验证通过后再决定。

---

## 4. 重编与部署

库目录自带 CMakeLists.txt 依赖 catkin(ROS)，本机无 ROS 环境时用 g++ 直接编（源码不引用 VersionConfig.h/oss_version.h，无缺失依赖）：

```bash
cd /home/jinnxf/workplace_jinn/HMCB_Demo/libMcbctrl_HMCB_20260421

# 1) 备份原版（必做）
cp libmcbctrl.so.1.0.0.131 libmcbctrl.so.1.0.0.131.orig

# 2) 重编（源文件清单与库 CMakeLists 的 SRC_CPP_FILE 一致，去掉未参与编译的项）
g++ -std=c++17 -fPIC -shared -O2 -DUNIX -DQT \
    src/MCBCtrl.cpp src/CB_bsp.cpp src/ECS_func.cpp src/ETH_func.cpp \
    src/FPGA_cfg.cpp src/IniFile.cpp src/MCBCtrlC.cpp src/MCBCtrlC_HMCB4.cpp \
    src/MCBThread.cpp src/RequestSocket.cpp src/nuctool.cpp src/logAdapter.cpp \
    -Isrc -Iinclude \
    -o libmcbctrl.so.1.0.0.131.fix \
    -lboost_system -lboost_thread -lpthread

# 3) 自检：导出符号完好
nm -D libmcbctrl.so.1.0.0.131.fix | grep SetMCBCommand

# 4) 替换（Qt 构建树 + dist 包两处都指向同一文件名的，替换库目录原件即可；
#    dist 包 dist/HMCB_Qt_*/lib/ 下的副本需同步替换）
mv libmcbctrl.so.1.0.0.131.fix libmcbctrl.so.1.0.0.131
cp libmcbctrl.so.1.0.0.131 "../HMCB_Qt/dist/HMCB_Qt_20260820/lib/"
```

HMCB_Qt 工程通过 `MCBCTRL_LIB_FILE = ../libMcbctrl_HMCB_20260421/libmcbctrl.so.1.0.0.131` 导入库（CMakeLists.txt:23），RPATH 指向库目录，替换文件后**无需重新配置 CMake**，直接重跑程序即可生效。

---

## 5. 验证方案

**A. fd 泄漏对照实验（核心，无需真设备过夜）**

用 `.99` 不存在 IP 代替拔电（connect 立即失败，节奏更快）：

```bash
# config/MCBCtrl.ini 的 IPADDR 改 192.111.111.99（用 /tmp 副本，勿动用户原配置）
# 启动程序 real 模式后：
pid=$(pgrep HMCB_Qt)
watch -n10 "ls /proc/$pid/fd | wc -l"
# 修复前：每 ~5s +1（1 小时 +700 左右）
# 修复后：数值稳定（±少量波动，不持续增长）
```

**B. 实机过夜复现（最终验收）**

1. 连接 CX1664DE 正常采集 → 设备断电 ≥2 小时（超过 fd 耗尽的 85 分钟临界）
2. 设备上电 → 观察状态栏/日志：应自动重连（CheckConnect 5s 节流，上电后 ≤10s 内 DVREADY）
3. 同时观察 `ls /proc/$pid/fd | wc -l` 全程不超 1024

**C. 回归**

- 正常流程：连接 → 采集 → 停止 → 关闭程序，无崩溃、无异常日志
- Qt 工程 14/14 单测全绿（库改动不影响 mock 路径，全量跑一遍确认）
- dist 包替换 .so 后 `run.sh` 冒烟

---

## 6. 风险与影响面

| 项 | 评估 |
|---|---|
| 正常连接/采集路径 | 修改 1/3 只动失败路径与存放方式，成功路径逻辑不变 |
| 单 MCB（实机 MCBNUM=1） | 修改 2/3 与原 erase+push_back 行为等价（槽位 0 独占） |
| 多 MCB | 修复了原索引错位缺陷，行为更正确（当前实机未用到） |
| Windows 版 HMBCtrl.dll | 同源缺陷，但 .dll 为厂商编译产物；库 src 具备跨平台条件编译，如需根治需另行构建 Windows 版（超出本方案范围） |
| 兼容性 | `_ETH_CTRL_socket` 全部使用点仍按 `[nMCB]` 索引，接口无变化；`ETH_CTRL_IsValid` 已有空指针分支 |
| 已知遗留（本方案不处理） | MCB_CMD_CLOSE 的 `delete gpMCBThread` UAF（MCBCtrl.cpp:213 附近）；MCBTimer 每 200ms 起 detached 线程无锁并发（修改 5 可缓解其堆积后果） |

---

## 7. 源码出处速查（本方案涉及）

| 主题 | 文件:行 |
|---|---|
| ETH_init（泄漏点） | ETH_func.cpp:57-136，失败返回 :128-132，push_back :134 |
| ETH_CTRL_close（条件不清理） | ETH_func.cpp:188-218 |
| ETH_close（同型函数，无调用者=死代码，不动） | ETH_func.cpp:170-186 |
| ETH_CTRL_write（裸索引访问） | ETH_func.cpp:220 起，getsockopt :245 |
| ETH_CTRL_IsValid（越界/空/is_open 检查） | ETH_func.cpp:687-715 |
| CheckConnect（5s 断链探测→重初始化） | MCBCtrlC.cpp:4008-4074 |
| CheckDVReadySts（10s 兜底重试） | MCBCtrlC.cpp:4085-4117 |
| MCBTimer（200ms 定时线程） | MCBCtrl.cpp:338-369 |
| 库 CMakeLists（catkin 依赖，重编绕开） | libMcbctrl_HMCB_20260421/CMakeLists.txt |
