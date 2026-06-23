# 🎒 电子吧唧 (Electronic Badge)

> ESP32-S3 + GC9A01 圆形 LCD + IT7259 触摸屏 可穿戴动画徽章

https://github.com/user-attachments/assets/77501322-4765-4628-bc50-cc4cd998ffbf

---

## 📦 硬件

| 组件 | 型号 | 接口 |
|------|------|------|
| 主控 | ESP32-S3 N16R8 | — |
| 屏幕 | TK0128F25K (GC9A01) | SPI |
| 触摸 | IT7259 | I2C (0x46) |
| 分辨率 | 240×240 圆形 | RGB565 |

## 🔌 引脚接线

| ESP32-S3 | 屏幕/触摸 | 功能 |
|----------|----------|------|
| GPIO 6 | SCLK | SPI 时钟 |
| GPIO 15 | MOSI/SDA | SPI 数据 |
| GPIO 7 | DC | 数据/命令 |
| GPIO 16 | CS | 片选 |
| GPIO 4 | SDA | I2C 触摸 |
| GPIO 5 | SCL | I2C 触摸 |

## 🏗️ 项目结构

```
badge/
├── platformio.ini          # PlatformIO 配置 (ESP-IDF 6.0.1)
├── partitions.csv          # Flash 分区表
├── sdkconfig.defaults      # Kconfig 默认值
├── data/
│   └── frames.bin          # 动画帧数据 (RGB565 原始二进制)
└── src/
    ├── CMakeLists.txt
    ├── main.c               # 主程序 (触摸 + 帧动画播放)
    ├── anim.h               # 帧数/尺寸元数据
    ├── esp_lcd_touch.h/c    # 触摸抽象层
    ├── LCD_TK0128F25K/      # GC9A01 屏幕驱动
    └── touch_CTP/           # IT7259 触摸驱动
```

## 🚀 编译 & 烧录

### 前置条件

- [VS Code](https://code.visualstudio.com/) + [PlatformIO 插件](https://platformio.org/)
- USB 连接 ESP32-S3 到电脑

### 步骤

```bash
# 1. 编译固件
pio run

# 2. 烧录固件
pio run --target upload

# 3. 烧录帧数据 (首次或换动画时)
python C:\Users\<用户名>\.platformio\packages\tool-esptoolpy\esptool.py \
  --chip esp32s3 --port COM8 --baud 460800 \
  write_flash 0x210000 data/frames.bin
```

## 🎨 自定义动画

### 准备图片序列

1. 将视频/GIF 导出为 PNG 序列，**240×240 像素**
2. 放到一个文件夹中，按 `frame_001.png`、`frame_002.png`... 命名

### 转换为帧数据

```python
from PIL import Image
import glob, os

files = sorted(glob.glob(r'你的文件夹\*.png'))
all_frames = bytearray()

for f in files[::N]:  # N = 每 N 帧取 1 帧
    img = Image.open(f).convert('RGB')
    img = img.resize((240, 240), Image.LANCZOS)
    for y in range(240):
        for x in range(240):
            r, g, b = img.getpixel((x, y))
            v = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
            # 大端序存储 (ESP32 小端 CPU)
            all_frames.append((v >> 8) & 0xFF)
            all_frames.append(v & 0xFF)

with open('data/frames.bin', 'wb') as f:
    f.write(all_frames)

print(f'{len(files[::N])} frames, {len(all_frames)/1024/1024:.1f} MB')
```

### 更新帧数

修改 `src/anim.h` 中的 `ANIM_FRAME_COUNT` 为实际帧数。

### 存储容量

| 分辨率 | 每帧大小 | 当前分区 (6MB) | 全 Flash (8MB) |
|--------|---------|---------------|---------------|
| 240×240 | 115 KB | ~54 帧 | ~72 帧 |
| 120×120 | 29 KB | ~210 帧 | ~280 帧 |

## 🧠 软件架构

```
┌──────────────────┐     ┌──────────────────┐
│   touch_task     │     │  display_task    │
│   (FreeRTOS)     │     │   (FreeRTOS)     │
│                  │     │                  │
│  读取 I2C 触摸   │────→│  读取 shared     │
│  识别手势/点击   │     │  state flags     │
│  更新 UI state   │     │                  │
└──────────────────┘     │  从 flash 分区    │
                         │  读取当前帧      │
                         │  绘制到 LCD      │
                         │  循环播放        │
                         └──────────────────┘
```

## 🎮 手势交互

| 手势 | 效果 |
|------|------|
| 单击/双击 | 缩放动画 |
| 上下左右滑动 | 拖拽图标 |

## 📄 License

MIT

---

*Built with ESP-IDF 6.0.1 + PlatformIO*
