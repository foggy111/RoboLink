# rl_hal 串口帧

模拟电机驱动和以后的真实串口都使用同一套二进制帧。多字节整数除 CRC 外按小端存放，CRC 本身也是小端（低字节在前），这和常见的 Modbus RTU 一致。

## 布局

```
偏移    长度    字段
0       1       魔数 0xAA
1       1       魔数 0x55
2       1       len，payload 的字节数，0–255
3       1       cmd
4       len     payload
4+len   2       CRC-16/MODBUS，低字节在前
```

一帧总长度是 `6 + len`。`len` 不包括魔数、命令和 CRC。

CRC-16/MODBUS 参数：

- 多项式 `0xA001`（反射形式，对应 `0x8005`）
- 初值 `0xFFFF`
- 输入、输出都不反射以外的额外处理；结果不异或
- 覆盖范围：魔数、len、cmd、payload，**不包括** CRC 自身

命令：

| cmd | 名字 | payload |
| --- | --- | --- |
| `0x01` | 设置轮速 | 两个 `int16` 小端：左轮 mm/s、右轮 mm/s |
| `0x02` | 心跳 | 空 |
| `0x03` | 读编码器 | 请求为空；响应格式留到 M3 |

## 例子

左轮 `100 mm/s`（`0x0064`），右轮 `-50 mm/s`（`0xFFCE`）：

```
AA 55 04 01 64 00 CE FF F8 5D
```

- `04` 是 payload 长度
- `01` 是设置轮速
- `64 00` 是 100
- `CE FF` 是 -50
- `F8 5D` 是 CRC `0x5DF8`

编解码实现在 `rl_hal/include/rl_hal/serial_frame.hpp`。`FrameParser` 会在字节流里丢掉噪声、处理半包和粘包，算法和 `rl_transport::Buffer` 是同一类问题，只是帧头换成了魔数加 CRC。

## 还没做的部分

`SimMotorDriver` 只在内存里保存最近一帧。M3 要把它接到伪终端或真实串口：周期下发轮速，并解析编码器回包。
