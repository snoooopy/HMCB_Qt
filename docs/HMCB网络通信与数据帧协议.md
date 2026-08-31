# HMCB 网络通信协议与数据帧协议

> 适用固件/库：libmcbctrl.so Ver-1.3.3（libMcbctrl_HMCB_20260421）
> 取证来源：库源码 `src/`（ETH_func.cpp、MCBCtrlC.cpp、CB_bsp.h、Config.h、MCB_param.h、memory_map.h、CmdMap.txt）
> 行号均指上述库源文件。库源为 ISO-8859/CRLF 编码，grep 时须加 `-a`。

---

## 1. 概述与拓扑

```
┌──────────┐  TCP 1024 (控制)   ┌─────────────────┐
│   PC     │◄──────────────────►│  HMCB 控制盒     │
│ (上位机) │  TCP 1028 (数据)   │ (EDAQ FPGA +    │
│          │◄──────────────────►│  DB/MB 子板链)  │
└──────────┘                    └─────────────────┘
```

- PC 与 HMCB 控制盒之间共 **2 条 TCP 连接**：
  - **控制通道**：端口 **1024**（`ETH_CTRL_PORT`，Config.h:11），承载 RBCP 寄存器读写（命令下发、FPGA 配置、参数设置）。
  - **数据通道**：端口 **1028**（`ETH_DATA_PORT`，Config.h:12），承载采集数据流（SC 包）。
- 默认 IP `192.111.111.211`（MCBCtrl.ini `[CONNECTION] IPADDR`）。
- 两条连接均基于 boost::asio tcp socket，并设置 **SO_KEEPALIVE**：idle 6 s / interval 6 s / 探测次数 60（ETH_func.cpp）。

---

## 2. 控制通道协议（RBCP over TCP 1024）

### 2.1 包格式

`ETH_func.cpp:30-39`：

```c
typedef struct ETH_CTRL_packet_format {
    INT8U  ver_type;   // 固定 0xFF
    INT8U  cmd_flag;   // 0x00 = 写，0xC0 = 读
    INT8U  ID;         // 包序号，自增
    INT8U  length;     // 数据长度（字节）
    INT32U addr;       // 寄存器地址，htonl 大端
    INT8U  buf[256];   // 数据
} RBCP_PKG_TYPE;
```

即 **8 字节头 + 最多 256 字节数据**。多字节字段大端（网络序）。

### 2.2 读/写语义

| cmd_flag | 操作 | 说明 |
|---|---|---|
| `0x00` | 写寄存器 | buf 内为待写数据 |
| `0xC0` | 读寄存器 | 应答包 buf 内为寄存器当前值 |

- 写操作带重试循环：`gWrRdReptN = 10`（读写重试次数）、`gCmdReptN = 1`（命令重试次数）。
- `ID` 字段为自增包序号，用于请求/应答配对。

### 2.3 寄存器寻址空间（memory_map.h）

EDAQ FPGA 总寻址 **16 MB**：

| 地址区间 | 用途 |
|---|---|
| `0x000000 – 0x00FFFF` | 寄存器区，每个寄存器 32 bit（`EDAQ_REG_BADDR = 0x000000`，`EDAQ_REG_NUM = 42`，即 Reg0~Reg41） |
| `0x010000 – 0x01FFFF` | RAM4K |

常用寄存器（基址 + n×4）：

| 寄存器 | 偏移 | 用途 |
|---|---|---|
| Reg0  | +0   | AUTO_RESET |
| Reg1  | +4   | GENERAL_CTRL |
| Reg2  | +8   | GENERAL_STATUS |
| Reg40 | +160 | FIRMWARE_VERSION（`ETH_FIRMWARE_VERSION_REG_ADDR`，CheckConnect 连接探测即读此寄存器） |
| Reg41 | +164 | （版本号延续） |

---

## 3. 应用命令与状态码（CmdMap 层）

RBCP 之上封装的应用命令（`SetMCBCommand` 入口，CmdMap.txt 全表节选）：

| 命令 | 值 | 参数 |
|---|---|---|
| MCB_CMD_INITIAL | 0x0100 | — |
| MCB_CMD_CONNECTMCB | 0x0101 | — |
| MCB_CMD_CLOSE | 0x0110 | — |
| MCB_CMD_SETMODE | 0x0200 | 模式 |
| MCB_CMD_START | 0x0300 | 包数 |
| MCB_CMD_STOP | 0x0301 | — |
| MCB_CMD_TRIGSET | 0x0400 | 0=内触发，1=外触发 |
| MCB_CMD_AOUTDASET | 0x8100 | AOUT 数据 |

### 3.1 回调消息

| 消息 | 值 | wParam（类别） | lParam |
|---|---|---|---|
| MCB_MSG_STATUS | WM_USER+0x100 | MCB_DVSTS 0x0100 / MCB_DVREADY 0x0101 / MCB_DVERR 0x0200 / MCB_STOPOK 0x0311 | `MCB号<<16 + 码` |
| MCB_MSG_DATA | WM_USER+0x200 | — | 数据帧指针 |

### 3.2 状态码表

| 码 | 含义 |
|---|---|
| 0x0100 | 连接中 |
| 0x0101 | 连接已建立 |
| 0x0110 | 连接失败 |
| 0x0200 | MB FPGA OK |
| 0x0201 | ADMB OK |
| 0x0210 | bin 文件错误 |
| 0x0300 | Start OK |
| 0x1111 | socket 失败 |
| 0x1110 | 运行中收到设置类命令 |
| 0x1210 | 热包超时（运行态） |
| 0x1211 | 热包超时（停止态） |

---

## 4. 数据帧协议（数据通道 TCP 1028）

数据流为 **SC 包 → DB/CB 子包 → 通道数据** 三级结构。

### 4.1 SC 包头（10 字节，MCBCtrlC.cpp:8832-8850）

| 偏移 | 长度 | 字段 | 说明 |
|---|---|---|---|
| 0 | 2 | 引导码 | `0xAA55`，大端 |
| 2 | 1 | 保留 | 补零 |
| 3 | 3 | 数据包总长度 | 3 字节大端（含 10 字节头本身） |
| 6 | 1 | DB line index | 0~3 |
| 7 | 1 | CMD code | 必须为 `0x08`（DB_DAQ_PKG） |
| 8 | 2 | 数据包编号 | 2 字节大端 |

- 头部校验失败时解析指针前移 1 字节重新搜索引导码；`0xAA55` 丢失超过 `65535×2` 次则退出解析线程。
- 接收缓存中**累积 ≥2 个完整包长**才触发解析线程（MCBCtrlC.cpp:5540 附近）。

### 4.2 包尺寸公式（MCBCtrlC.cpp:5529、5540-5557）

```
ADFormat_Bit = (m_nADBit > 16) ? 20 : 16          // AD 位宽折算
OneDBSize    = 6 + (rowNum × colNum) × ADFormat_Bit / 8 + 1   // DB 子包总长
CBDataPackSize = 6 + 12 + 1                       // CB 子包 = 6 头 + 12 数据 + 1 CRC = 19
SC 包长      = 10 + nDMNum × OneDBSize + 19
```

**实例（CX1664DE，1 DM × 64 行 × 2 列 = 128 通道，16 bit）：**

```
OneDBSize = 6 + 128×16/8 + 1 = 263
SC 包长   = 10 + 1×263 + 19 = 292 字节
实测包率 ≈ 37 包/秒
```

### 4.3 DB / CB 子包头（6 字节，MCBCtrlC.cpp:8960-8990）

| 偏移 | 字段 | 说明 |
|---|---|---|
| 0 | 包类型 | CB_bsp.h:125-130：`0x08`=DB_DAQ_PKG，`0xE0`=DB_ERR_PKG，`0xC0`=CB_PKG |
| 1 | 长度低 8 位 | |
| 2 | 高 4 位包编号 + 低 4 位长度高 4 位 | |
| 3 | 板编号 | CB 或 DB 编号 |
| 4 | logic_err1 | |
| 5 | logic_err2 | |

子包体 = `xB_pkg_size` 字节数据 + **1 字节 CRC-8**。
注：库解析时仅提取 CRC 字段存入帧头（DB_crc_code），**不做校验比较**。

### 4.4 CB 包数据（12 字节）

| 字节 | 内容 |
|---|---|
| Data0 | 高 4 位 SDCLK 错误类型 / 低 4 位 SDCLK 宽度（按 DB line 独立） |
| Data1 | 保留 |
| Data2 | 高 4 位 DDI |
| Data3 | 保留 |
| Data4~11 | 用户数据（时间戳、触发计数等，见 §5 映射） |

### 4.5 通道数据编码（MCBCtrlC.cpp:9408-9440）

**16 bit：2 字节/通道，低字节在前（小端）。**

**20 bit：5 字节装 2 个通道。** 例：通道 A=0x54321、通道 B=0x98765，线上字节序为 `54 32 19 87 65`——即两通道拼接成 40 bit 后按字节交错切分，解析端用 nTempCh 拼接还原：

```
发送侧: A(20bit) ++ B(20bit) → 5 字节流: 54 32 19 87 65
接收侧: 字节流 → 40 bit → 低 20 bit = 通道 A，高 20 bit = 通道 B
```

### 4.6 CB 数据 → 应用帧头映射（MCBCtrlC.cpp:9769-9850）

库把 CB 包 12 字节折算成应用帧头（`MCBData.head_use`）字段：

| 帧头字段 | 来源/公式 |
|---|---|
| nError_DI | `((~CB_data[2]) & 0xF0) << 4`（DDI 取反；godang_S4 机型位交换） |
| ain0 | CB_data[4..6] 拼 24 bit；最高位为符号：1→取 0x7FFFFF，0→加 0x800000；<24bit 时右移 `24-m_nADBit` 适配 |
| ain1 | 0 |
| ptsmdata_h | CB_data[0..3]（系统保留） |
| ptsmdata_l | nPulseCounter（见下） |
| accEnergy | `(nError_DI & 0x0200) == 0 ? 1 : 0` |
| scounter | 软件包序号（从 1 起） |
| pcounter | `m_bHWSerialNumberMode` 为真取硬件触发数，否则取软件包序号 |
| sscnt | Update_pkg_n（已解析包数） |
| regError_DI | m_bAdjAinDi 时 AIN/DI 错位 1 触发调整 |

其中硬件触发数：

```
nPulseCounter = CB_data[8..11] 大端 32 位
              − SC_1st_pkg_TriggCnt   // 首包触发计数基线
              − INVALID_DATA_PACKET_NUM (= 6, MCB_param.h:49)
              + m_nDiscardNum + 1
```

---

## 5. 保活与超时

- **热包机制**：每收到有效数据刷新 `m_timehot` 时间戳；`CheckDVReadySts`（MCBCtrlC.cpp:4028）检测 **5 秒无活动**判热包超时 → 触发重连（状态码 0x1210/0x1211）。
- **TCP keepalive**：SO_KEEPALIVE idle 6 s / interval 6 s / count 60，作为底层兜底。
- 已知库缺陷（观察项）：空闲约 15 s 后 CheckConnect 重连路径存在段错误（崩溃地址 0x60D42），上位机侧规避方式为避免长时间挂在 idle 探测状态。

---

## 6. 一次典型会话流程

```
1. CONNECT/INITIAL
   ├─ TCP 1024 建立，RBCP 读 Reg40 固件版本（连接探测）
   ├─ 控制通道下发 FPGA bin（DB/MB 配置）
   └─ 状态回调: 0x0100 → 0x0101 (DVREADY)
2. SETMODE / TRIGSET / AOUTDASET（RBCP 写寄存器）
3. START(包数)
   ├─ TCP 1028 数据流: SC 包(0xAA55...) 连续到达
   ├─ 每包: 10B SC头 + n×DB子包 + 1×CB子包
   └─ 库解析→MCB_MSG_DATA 回调→上位机 UI
4. STOP
   └─ 状态回调 0x0311 (STOPOK)
5. 热包超时(5s) / CLOSE
```

---

## 7. 源码出处速查

| 主题 | 文件:行 |
|---|---|
| 端口定义 | Config.h:11-12 |
| RBCP 包结构/读写/keepalive | ETH_func.cpp:30-39 及读写函数 |
| 寄存器空间 | memory_map.h（EDAQ_REG_BADDR/EDAQ_REG_NUM/RAM4K） |
| 命令/消息/状态码全表 | CmdMap.txt |
| ADFormat_Bit | MCBCtrlC.cpp:5529 |
| 包尺寸公式 | MCBCtrlC.cpp:5540-5557 |
| SC 包头解析 | MCBCtrlC.cpp:8832-8850 |
| DB/CB 子包头解析 | MCBCtrlC.cpp:8960-8990 |
| 包类型定义 | CB_bsp.h:125-130 |
| 16/20bit 通道解包 | MCBCtrlC.cpp:9408-9440 |
| CB→帧头映射 | MCBCtrlC.cpp:9769-9850 |
| 热包超时 | MCBCtrlC.cpp:4028 |
| INVALID_DATA_PACKET_NUM | MCB_param.h:49 |
