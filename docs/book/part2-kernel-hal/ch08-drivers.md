# Chapter 8: Driver Framework

*Srikanth Patchava & EmbeddedOS Contributors*

---

## 8.1 Overview

The EoS driver framework provides a structured approach to hardware driver development
with a well-defined lifecycle, power management integration, and device tree support.

The framework consists of three components:

| Component | Header | Purpose |
|-----------|--------|---------|
| **Driver Model** | `driver.h` | Registration, probe/remove lifecycle, matching |
| **Driver Framework** | `drivers.h` | Unified open/close/read/write/ioctl interface |
| **Device Tree** | `devicetree.h` | Hardware description parsing (.dtb) |

## 8.2 Driver Architecture

```
+------------------------------------------------------------+
|                    Application Code                        |
|              eos_driver_find("uart0")                      |
+------------------------------------------------------------+
|                  Driver Framework                          |
|     register -> probe -> open -> read/write -> close       |
+----------+----------+----------+----------+----------------+
|  GPIO    |  UART    |  SPI     |  I2C     |   ... (19     |
|  Driver  |  Driver  |  Driver  |  Driver  |   classes)    |
+----------+----------+----------+----------+----------------+
|                    Device Tree                             |
|            (hardware description from .dtb)                |
+------------------------------------------------------------+
|                    Hardware                                 |
+------------------------------------------------------------+
```

## 8.3 Driver Classes

EoS defines 19 driver classes covering all peripheral types:

| Class Enum | Value | Category |
|-----------|-------|----------|
| `EOS_DRV_CLASS_GPIO` | 0 | Digital I/O |
| `EOS_DRV_CLASS_UART` | 1 | Serial |
| `EOS_DRV_CLASS_SPI` | 2 | Serial |
| `EOS_DRV_CLASS_I2C` | 3 | Serial |
| `EOS_DRV_CLASS_CAN` | 4 | Bus |
| `EOS_DRV_CLASS_PWM` | 5 | Analog |
| `EOS_DRV_CLASS_ADC` | 6 | Analog |
| `EOS_DRV_CLASS_DAC` | 7 | Analog |
| `EOS_DRV_CLASS_TIMER` | 8 | Timing |
| `EOS_DRV_CLASS_DMA` | 9 | System |
| `EOS_DRV_CLASS_ETHERNET` | 10 | Network |
| `EOS_DRV_CLASS_USB` | 11 | Bus |
| `EOS_DRV_CLASS_SDIO` | 12 | Storage |
| `EOS_DRV_CLASS_DISPLAY` | 13 | Graphics |
| `EOS_DRV_CLASS_SENSOR` | 14 | Input |
| `EOS_DRV_CLASS_STORAGE` | 15 | Storage |
| `EOS_DRV_CLASS_NETWORK` | 16 | Network |
| `EOS_DRV_CLASS_AUDIO` | 17 | Media |
| `EOS_DRV_CLASS_CAMERA` | 18 | Media |
| `EOS_DRV_CLASS_CUSTOM` | 0xFF | User-defined |

## 8.4 Driver Lifecycle

Every driver follows a well-defined state machine:

```
             register()
                |
                v
          +-----------+
          | UNLOADED  |
          +-----+-----+
                |  eos_drv_register()
                v
          +-----------+
          |  LOADED   |
          +-----+-----+
                |  eos_drv_probe()
                v
          +-----------+     probe fails     +-----------+
          |  PROBING  | ------------------> |   ERROR   |
          +-----+-----+                     +-----------+
                |  probe succeeds
                v
          +-----------+
          |   BOUND   |  <-- fully operational
          +-----+-----+
                |  eos_drv_unbind()
                v
          +-----------+
          |  LOADED   |  (can be re-probed)
          +-----------+
```

### State Definitions

| State | Value | Description |
|-------|-------|-------------|
| `EOS_DRV_UNLOADED` | 0 | Not registered |
| `EOS_DRV_LOADED` | 1 | Registered but not bound to hardware |
| `EOS_DRV_PROBING` | 2 | Currently executing probe function |
| `EOS_DRV_BOUND` | 3 | Successfully bound, operational |
| `EOS_DRV_ERROR` | 4 | Probe failed |

## 8.5 The Driver Structure

The `EosDriver` structure is the central data type. Each driver instance contains
identification, operations, matching info, and runtime data:

```c
typedef struct eos_driver {
    // Identification
    char            name[EOS_DRV_NAME_MAX];  // Unique driver name
    uint32_t        version;                  // Driver version
    EosDrvClass     class_id;                 // Peripheral class
    EosDrvState     state;                    // Current lifecycle state

    // Hardware matching
    EosDrvMatchInfo match;                    // vendor/device/compat

    // Operations (function pointers)
    int  (*probe)(struct eos_driver *drv, void *platform_data);
    void (*remove)(struct eos_driver *drv);
    int  (*suspend)(struct eos_driver *drv);
    int  (*resume)(struct eos_driver *drv);
    int  (*ioctl)(struct eos_driver *drv, uint32_t cmd, void *arg);

    // Runtime data
    void    *priv_data;       // Driver-private data
    void    *platform_data;   // Platform-specific data
    int      ref_count;       // Reference count
    int      irq_num;         // Assigned IRQ number
    uint32_t base_addr;       // Memory-mapped base address
    uint32_t reg_size;        // Register region size
} EosDriver;
```

### Match Info Structure

```c
typedef struct {
    uint16_t vendor_id;       // Vendor identifier
    uint16_t device_id;       // Device identifier
    uint32_t compat_hash;     // Hash of compatibility string
    char     compat_str[32];  // e.g., "st,stm32-uart"
} EosDrvMatchInfo;
```

## 8.6 Driver Registry API

The driver registry manages all registered drivers:

### Registration and Lookup

| Function | Signature | Description |
|----------|-----------|-------------|
| `eos_drv_init` | `int eos_drv_init(EosDrvRegistry *reg)` | Initialize registry |
| `eos_drv_shutdown` | `void eos_drv_shutdown(EosDrvRegistry *reg)` | Unbind all, shutdown |
| `eos_drv_register` | `int eos_drv_register(EosDrvRegistry *reg, EosDriver *drv)` | Register a driver |
| `eos_drv_unregister` | `int eos_drv_unregister(EosDrvRegistry *reg, const char *name)` | Unregister by name |
| `eos_drv_find` | `EosDriver *eos_drv_find(EosDrvRegistry *reg, const char *name)` | Find by name |
| `eos_drv_find_by_class` | `EosDriver *eos_drv_find_by_class(reg, cls, idx)` | Find by class |
| `eos_drv_find_by_match` | `EosDriver *eos_drv_find_by_match(reg, match)` | Find by match info |

### Lifecycle Operations

| Function | Description |
|----------|-------------|
| `eos_drv_probe(reg, name, platform_data)` | Probe a specific driver |
| `eos_drv_probe_all(reg)` | Probe all loaded drivers |
| `eos_drv_unbind(reg, name)` | Unbind a bound driver |

### Introspection

| Function | Description |
|----------|-------------|
| `eos_drv_count(reg)` | Total registered drivers |
| `eos_drv_count_by_class(reg, class)` | Count by peripheral class |
| `eos_drv_dump(reg)` | Print registry to stderr |
| `eos_drv_ioctl(reg, name, cmd, arg)` | Send ioctl to named driver |

## 8.7 Writing a Driver: Complete Example

Here is a complete example of an I2C temperature sensor driver:

```c
#include <eos/driver.h>
#include <stdio.h>

#define TMP102_TEMP_REG  0x00
#define TMP102_CONF_REG  0x01

typedef struct {
    uint8_t  i2c_port;
    uint16_t i2c_addr;
    float    last_temp;
} tmp102_data_t;

// --- Probe: initialize the hardware ---
static int tmp102_probe(EosDriver *drv, void *platform_data)
{
    tmp102_data_t *data = (tmp102_data_t *)drv->priv_data;
    if (!data) return -1;

    // Configure sensor: 12-bit resolution, continuous mode
    uint8_t conf[2] = { 0x60, 0xA0 };
    eos_i2c_write_reg(data->i2c_port, data->i2c_addr,
                       TMP102_CONF_REG, conf, 2);

    printf("[tmp102] Probed at I2C %d:0x%02X\n",
           data->i2c_port, data->i2c_addr);
    return 0;
}

// --- Remove: cleanup ---
static void tmp102_remove(EosDriver *drv)
{
    printf("[tmp102] Removed\n");
}

// --- Suspend: put sensor in shutdown mode ---
static int tmp102_suspend(EosDriver *drv)
{
    tmp102_data_t *data = (tmp102_data_t *)drv->priv_data;
    uint8_t conf[2] = { 0x61, 0xA0 };  // SD bit set
    eos_i2c_write_reg(data->i2c_port, data->i2c_addr,
                       TMP102_CONF_REG, conf, 2);
    return 0;
}

// --- Resume: wake sensor ---
static int tmp102_resume(EosDriver *drv)
{
    tmp102_data_t *data = (tmp102_data_t *)drv->priv_data;
    uint8_t conf[2] = { 0x60, 0xA0 };  // SD bit clear
    eos_i2c_write_reg(data->i2c_port, data->i2c_addr,
                       TMP102_CONF_REG, conf, 2);
    return 0;
}

// --- Ioctl: read temperature ---
#define TMP102_IOCTL_READ_TEMP  0x01

static int tmp102_ioctl(EosDriver *drv, uint32_t cmd, void *arg)
{
    if (cmd != TMP102_IOCTL_READ_TEMP || !arg) return -1;

    tmp102_data_t *data = (tmp102_data_t *)drv->priv_data;
    uint8_t raw[2];
    eos_i2c_read_reg(data->i2c_port, data->i2c_addr,
                      TMP102_TEMP_REG, raw, 2);

    int16_t temp = (raw[0] << 4) | (raw[1] >> 4);
    if (temp & 0x800) temp |= 0xF000;

    float *result = (float *)arg;
    *result = temp * 0.0625f;
    data->last_temp = *result;
    return 0;
}

// --- Driver definition ---
static tmp102_data_t sensor_data = {
    .i2c_port = 0,
    .i2c_addr = 0x48,
};

static EosDriver tmp102_driver = {
    .name     = "tmp102",
    .version  = 1,
    .class_id = EOS_DRV_CLASS_SENSOR,
    .match    = {
        .compat_str = "ti,tmp102",
    },
    .probe    = tmp102_probe,
    .remove   = tmp102_remove,
    .suspend  = tmp102_suspend,
    .resume   = tmp102_resume,
    .ioctl    = tmp102_ioctl,
    .priv_data = &sensor_data,
};

// --- Usage ---
void app_init(void)
{
    EosDrvRegistry registry;
    eos_drv_init(&registry);

    eos_drv_register(&registry, &tmp102_driver);
    eos_drv_probe(&registry, "tmp102", NULL);

    float temp;
    eos_drv_ioctl(&registry, "tmp102", TMP102_IOCTL_READ_TEMP, &temp);
    printf("Temperature: %.2f C\n", temp);
}
```

## 8.8 Power Management

The driver framework integrates power management through `suspend` and `resume`
callbacks.

### System-Wide Suspend/Resume

```c
// Suspend all bound drivers (in reverse registration order)
int suspended = eos_drv_suspend_all(&registry);
printf("%d drivers suspended\n", suspended);

// Enter low-power mode...

// Resume all drivers (in registration order)
int resumed = eos_drv_resume_all(&registry);
printf("%d drivers resumed\n", resumed);
```

### Suspend/Resume Order

Suspend processes drivers in **reverse registration order** (last registered suspends
first). Resume processes in **forward order**. This ensures dependencies are respected:

```
Registration:  gpio -> i2c -> sensor -> display
Suspend order: display -> sensor -> i2c -> gpio
Resume order:  gpio -> i2c -> sensor -> display
```

## 8.9 Device Tree Integration

EoS includes a device tree parser for describing hardware topology in a
vendor-neutral format, compatible with the standard Devicetree Specification.

### Device Tree Structure

```c
typedef struct eos_dt_node {
    char               name[EOS_DT_NAME_MAX];
    EosDtProp          props[EOS_DT_MAX_PROPS];
    int                prop_count;
    struct eos_dt_node *parent;
    struct eos_dt_node *children[16];
    int                child_count;
    uint32_t           phandle;
} EosDtNode;

typedef struct {
    EosDtNode  nodes[EOS_DT_MAX_NODES];
    int        node_count;
    EosDtNode  *root;
    uint32_t   version;
    uint32_t   boot_cpuid;
    char       model[64];
    char       compatible[128];
} EosDeviceTree;
```

### Key Constants

| Macro | Value | Description |
|-------|-------|-------------|
| `EOS_DT_MAX_NODES` | 128 | Maximum nodes in tree |
| `EOS_DT_MAX_PROPS` | 16 | Maximum properties per node |
| `EOS_DT_NAME_MAX` | 64 | Maximum name length |
| `EOS_DT_PROP_MAX` | 256 | Maximum property data size |
| `EOS_DT_MAGIC` | `0xD00DFEED` | DTB magic number |

### Device Tree API

| Function | Description |
|----------|-------------|
| `eos_dt_parse(dt, dtb, size)` | Parse a flattened device tree blob |
| `eos_dt_find(dt, path)` | Find node by path |
| `eos_dt_find_compatible(dt, compat)` | Find node by compatible string |
| `eos_dt_find_by_phandle(dt, phandle)` | Find node by phandle |
| `eos_dt_get_prop(node, name)` | Get raw property |
| `eos_dt_get_u32(node, name, val)` | Read 32-bit property |
| `eos_dt_get_string(node, name, buf, len)` | Read string property |
| `eos_dt_get_reg(node, addr, size)` | Read reg property (base + size) |
| `eos_dt_get_irq(node, index)` | Read interrupt number |

### Example: Parsing a Device Tree

```c
EosDeviceTree dt;
extern const uint8_t _dtb_start[];
extern const uint32_t _dtb_size;

// Parse the compiled device tree blob
eos_dt_parse(&dt, _dtb_start, _dtb_size);

// Find UART node
EosDtNode *uart = eos_dt_find(&dt, "/soc/uart@40004000");
if (uart) {
    uint32_t base, size;
    eos_dt_get_reg(uart, &base, &size);
    printf("UART base=0x%08X size=0x%X\n", base, size);

    int irq = eos_dt_get_irq(uart, 0);
    printf("UART IRQ=%d\n", irq);
}

// Find all nodes with a specific compatible string
EosDtNode *sensor = eos_dt_find_compatible(&dt, "ti,tmp102");
if (sensor) {
    uint32_t addr;
    eos_dt_get_u32(sensor, "reg", &addr);
    printf("TMP102 at I2C address 0x%02X\n", addr);
}
```

### Example Device Tree Source

```dts
/ {
    model = "EoS Dev Board";
    compatible = "eos,devboard";

    soc {
        uart@40004000 {
            compatible = "st,stm32-uart";
            reg = <0x40004000 0x400>;
            interrupts = <38>;
            clock-frequency = <84000000>;
            status = "okay";
        };

        i2c@40005400 {
            compatible = "st,stm32-i2c";
            reg = <0x40005400 0x400>;
            clock-frequency = <400000>;
            status = "okay";

            tmp102@48 {
                compatible = "ti,tmp102";
                reg = <0x48>;
            };
        };

        gpio@40020000 {
            compatible = "st,stm32-gpio";
            reg = <0x40020000 0x400>;
            gpio-cells = <2>;
        };
    };
};
```

## 8.10 Driver-Device Tree Integration

Combine the driver framework with device tree for automatic hardware discovery:

```c
void auto_probe_drivers(EosDrvRegistry *reg, EosDeviceTree *dt)
{
    for (int i = 0; i < eos_drv_count(reg); i++) {
        EosDriver *drv = reg->drivers[i];
        if (!drv || drv->state != EOS_DRV_LOADED) continue;

        // Match driver against device tree
        EosDtNode *node = eos_dt_find_compatible(
            dt, drv->match.compat_str);
        if (node) {
            uint32_t base, size;
            eos_dt_get_reg(node, &base, &size);

            drv->base_addr = base;
            drv->reg_size  = size;
            drv->irq_num   = eos_dt_get_irq(node, 0);

            eos_drv_probe(reg, drv->name, node);
            printf("Auto-probed %s at 0x%08X\n", drv->name, base);
        }
    }
}
```

## 8.11 The Unified Driver Framework

In addition to the registry model, EoS provides a higher-level driver framework with
a standard `open/close/read/write/ioctl` interface, similar to POSIX file operations.

### Driver States (Framework)

| State | Value | Description |
|-------|-------|-------------|
| `EOS_DRV_STATE_UNINIT` | 0 | Not initialized |
| `EOS_DRV_STATE_READY` | 1 | Initialized, ready to open |
| `EOS_DRV_STATE_ACTIVE` | 2 | Opened and operational |
| `EOS_DRV_STATE_SUSPEND` | 3 | Suspended (low power) |
| `EOS_DRV_STATE_ERROR` | 4 | Error state |

### Framework API

| Function | Description |
|----------|-------------|
| `eos_driver_register(drv)` | Register a driver |
| `eos_driver_unregister(name)` | Unregister by name |
| `eos_driver_find(name)` | Find by name |
| `eos_driver_find_by_type(type, instance)` | Find by type and instance |
| `eos_driver_init_all()` | Initialize all registered drivers |
| `eos_driver_deinit_all()` | Deinitialize all drivers |
| `eos_driver_open(drv)` | Open a driver for use |
| `eos_driver_close(drv)` | Close a driver |
| `eos_driver_read(drv, buf, len)` | Read data from driver |
| `eos_driver_write(drv, buf, len)` | Write data to driver |
| `eos_driver_ioctl(drv, cmd, arg)` | Device-specific control |
| `eos_driver_suspend_all()` | Suspend all active drivers |
| `eos_driver_resume_all()` | Resume all suspended drivers |
| `eos_driver_count()` | Total registered drivers |
| `eos_driver_list(out, max, count)` | List all drivers |

### Framework Lifecycle

```
register -> init -> open -> read/write/ioctl -> close -> deinit -> unregister
              |                                   |
              v                                   v
           READY                               READY
              |                                   |
              +--- open --> ACTIVE -- close ------+
                              |
                          suspend --> SUSPEND -- resume --> ACTIVE
```

## 8.12 Best Practices

### Driver Development Guidelines

| Guideline | Rationale |
|-----------|-----------|
| Always implement `probe` and `remove` | Clean lifecycle management |
| Implement `suspend`/`resume` for battery devices | Enables system-wide power management |
| Use `priv_data` for driver state | Keeps the driver struct generic |
| Set `match.compat_str` | Enables device tree auto-matching |
| Check `state` before operations | Prevents use-after-free |
| Use `ioctl` for device-specific commands | Keeps the core API clean |

### Common Patterns

**Resource acquisition in probe:**
```c
static int my_probe(EosDriver *drv, void *pdata)
{
    // 1. Allocate private data
    my_data_t *data = malloc(sizeof(my_data_t));
    if (!data) return -1;

    // 2. Map registers
    data->regs = (volatile uint32_t *)drv->base_addr;

    // 3. Configure hardware
    data->regs[CTRL_REG] = ENABLE_BIT;

    // 4. Register interrupt
    eos_irq_register(drv->irq_num, my_isr, 5);

    drv->priv_data = data;
    return 0;
}
```

**Clean removal:**
```c
static void my_remove(EosDriver *drv)
{
    my_data_t *data = (my_data_t *)drv->priv_data;

    // 1. Disable hardware
    data->regs[CTRL_REG] = 0;

    // 2. Unregister interrupt
    eos_irq_unregister(drv->irq_num);

    // 3. Free private data
    free(data);
    drv->priv_data = NULL;
}
```

## 8.13 Summary

| Component | Purpose |
|-----------|---------|
| `EosDriver` struct | Driver identity, operations, and runtime data |
| `EosDrvRegistry` | Centralized driver management |
| Probe/Remove | Hardware binding lifecycle |
| Suspend/Resume | System-wide power management |
| Device Tree | Vendor-neutral hardware description |
| Match Info | Automatic driver-to-device binding |
| Framework API | POSIX-like open/close/read/write interface |

The driver framework ties together the HAL (Chapter 4-5), the kernel (Chapter 6),
and the multicore subsystem (Chapter 7) into a coherent model for managing hardware
across the full range of EoS-supported platforms.

---

*End of Part II: Kernel and HAL*
