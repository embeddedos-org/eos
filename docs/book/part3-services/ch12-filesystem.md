# Chapter 12: Filesystem

**Author:** Srikanth Patchava & EmbeddedOS Contributors

---

## 12.1 Overview

The EoS filesystem layer (eos/filesystem.h) provides a unified file API
that abstracts over multiple storage backends -- FAT, LittleFS, SPIFFS, and
RAM disk. Applications use a single set of calls regardless of the
underlying flash or SD card hardware, gated by EOS_ENABLE_FILESYSTEM.

```
+----------------------------------------------------+
|                 Application                        |
|       eos_fs_open / read / write / close           |
+----------------------------------------------------+
|              <eos/filesystem.h>                    |
+----------+-----------+-----------+-----------------+
|   FAT    | LittleFS  |  SPIFFS   |   RAMFS         |
| (SD card)| (NOR/NAND)| (SPI NOR) | (testing)       |
+----------+-----------+-----------+-----------------+
|                 Flash HAL / Block Driver            |
+----------------------------------------------------+
```

## 12.2 Filesystem Types

| Enum              | Backend     | Best For                       |
|-------------------|-------------|--------------------------------|
| EOS_FS_FAT        | FAT12/16/32 | SD cards, large media          |
| EOS_FS_LITTLEFS   | LittleFS    | NOR/NAND flash, wear leveling  |
| EOS_FS_SPIFFS     | SPIFFS      | Small SPI NOR flash            |
| EOS_FS_RAMFS      | RAM disk    | Testing, volatile scratch      |

## 12.3 Configuration

```c
typedef struct {
    eos_fs_type_t type;       /* Filesystem type           */
    uint8_t       flash_id;   /* HAL flash device ID       */
    uint32_t      base_addr;  /* Start address in flash    */
    uint32_t      size;       /* Partition size in bytes   */
} eos_fs_config_t;
```

### 12.3.1 Initialization Example

```c
#include <eos/filesystem.h>

void fs_setup(void)
{
    eos_fs_config_t cfg = {
        .type      = EOS_FS_LITTLEFS,
        .flash_id  = 0,            /* Internal flash   */
        .base_addr = 0x00100000,   /* 1 MB offset      */
        .size      = 0x00080000,   /* 512 KB partition  */
    };

    int rc = eos_fs_init(&cfg);
    if (rc != 0) {
        printf("FS init failed: %d\n", rc);
        eos_fs_format();
        eos_fs_init(&cfg);
    }
}
```

## 12.4 Open Flags

Flags can be OR-ed together to control file open behavior:

| Flag             | Value   | Description                         |
|------------------|---------|-------------------------------------|
| EOS_O_READ       | 0x01    | Open for reading                    |
| EOS_O_WRITE      | 0x02    | Open for writing                    |
| EOS_O_CREATE     | 0x04    | Create file if it does not exist    |
| EOS_O_APPEND     | 0x08    | Append writes to end of file        |
| EOS_O_TRUNC      | 0x10    | Truncate file to zero length        |

## 12.5 File Operations

### 12.5.1 Write and Read

```c
void file_rw_example(void)
{
    /* Create and write */
    eos_file_t fd = eos_fs_open("/config.json",
                                EOS_O_WRITE | EOS_O_CREATE | EOS_O_TRUNC);
    if (fd == EOS_FILE_INVALID) return;

    const char *data = "{\"interval\":5000}";
    eos_fs_write(fd, data, strlen(data));
    eos_fs_sync(fd);   /* Flush to flash */
    eos_fs_close(fd);

    /* Read back */
    fd = eos_fs_open("/config.json", EOS_O_READ);
    char buf[128];
    int n = eos_fs_read(fd, buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = 0;
        printf("Config: %s\n", buf);
    }
    eos_fs_close(fd);
}
```

### 12.5.2 Seek and Tell

```c
void seek_example(eos_file_t fd)
{
    /* Jump to byte 100 from start */
    eos_fs_seek(fd, 100, EOS_SEEK_SET);

    /* Get current position */
    uint32_t pos;
    eos_fs_tell(fd, &pos);
    printf("Position: %u\n", pos);

    /* Jump to 50 bytes before end */
    eos_fs_seek(fd, -50, EOS_SEEK_END);
}
```

### 12.5.3 Truncate

```c
/* Shrink a log file to 1 KB */
eos_file_t fd = eos_fs_open("/log.txt", EOS_O_WRITE);
eos_fs_truncate(fd, 1024);
eos_fs_close(fd);
```

## 12.6 Directory Operations

```c
void directory_example(void)
{
    /* Create a directory */
    eos_fs_mkdir("/data");

    /* List directory contents */
    eos_dir_t dir = eos_fs_opendir("/data");
    if (dir == EOS_DIR_INVALID) return;

    eos_dirent_t entry;
    while (eos_fs_readdir(dir, &entry) == 0) {
        printf("%s %s  %u bytes\n",
               entry.is_dir ? "[DIR]" : "[FILE]",
               entry.name,
               entry.size);
    }
    eos_fs_closedir(dir);
}
```

### 12.6.1 Directory Entry Structure

```c
typedef struct {
    char     name[EOS_PATH_MAX];   /* File/directory name  */
    uint32_t size;                 /* Size in bytes        */
    bool     is_dir;               /* true if directory    */
} eos_dirent_t;
```

## 12.7 Path Operations

```c
/* Check existence */
if (eos_fs_exists("/config.json")) {
    printf("Config file found\n");
}

/* Rename / move */
eos_fs_rename("/config.json", "/config.json.bak");

/* Delete */
eos_fs_remove("/old_log.txt");
```

## 12.8 Filesystem Statistics

```c
eos_fs_stat_t stat;
eos_fs_stat(&stat);

printf("Total: %u KB\n", stat.total_bytes / 1024);
printf("Used:  %u KB\n", stat.used_bytes  / 1024);
printf("Free:  %u KB\n", stat.free_bytes  / 1024);
```

```c
typedef struct {
    uint32_t total_bytes;   /* Total filesystem capacity  */
    uint32_t used_bytes;    /* Bytes in use               */
    uint32_t free_bytes;    /* Bytes available            */
} eos_fs_stat_t;
```

## 12.9 System Limits

| Constant         | Value | Description                     |
|------------------|-------|---------------------------------|
| EOS_PATH_MAX     | 128   | Maximum path length in bytes    |
| EOS_FILE_MAX     | 8     | Maximum simultaneously open fds |

## 12.10 Configuration Persistence Pattern

A common pattern for storing device configuration:

```c
#include <eos/filesystem.h>
#include <eos/crypto.h>

typedef struct {
    uint32_t magic;         /* 0xC0FFEE01                 */
    uint32_t version;       /* Config schema version      */
    uint32_t interval_ms;   /* Sensor polling interval    */
    char     mqtt_host[64]; /* MQTT broker hostname       */
    uint16_t mqtt_port;     /* MQTT broker port           */
    uint32_t crc;           /* CRC-32 of preceding fields */
} device_config_t;

int config_save(const device_config_t *cfg)
{
    eos_file_t fd = eos_fs_open("/config.bin",
                                EOS_O_WRITE | EOS_O_CREATE | EOS_O_TRUNC);
    if (fd == EOS_FILE_INVALID) return -1;

    eos_fs_write(fd, cfg, sizeof(*cfg));
    eos_fs_sync(fd);
    eos_fs_close(fd);
    return 0;
}

int config_load(device_config_t *cfg)
{
    if (!eos_fs_exists("/config.bin")) return -1;

    eos_file_t fd = eos_fs_open("/config.bin", EOS_O_READ);
    if (fd == EOS_FILE_INVALID) return -1;

    int n = eos_fs_read(fd, cfg, sizeof(*cfg));
    eos_fs_close(fd);

    if (n != sizeof(*cfg) || cfg->magic != 0xC0FFEE01) {
        return -2;   /* Corrupt or wrong format */
    }

    /* Verify CRC */
    uint32_t expected = eos_crc32(0, cfg,
                                  sizeof(*cfg) - sizeof(uint32_t));
    if (expected != cfg->crc) return -3;

    return 0;
}
```

## 12.11 Filesystem Type Comparison

| Feature          | FAT        | LittleFS    | SPIFFS     | RAMFS     |
|------------------|------------|-------------|------------|-----------|
| Wear leveling    | No         | Yes         | Yes        | N/A       |
| Directories      | Yes        | Yes         | No         | Yes       |
| Power-safe       | No         | Yes (CoW)   | Partial    | No        |
| Max file size    | 4 GB       | Partition   | Partition  | RAM       |
| RAM overhead     | ~4 KB      | ~2 KB       | ~1 KB      | Minimal   |
| Best for         | SD cards   | NOR flash   | Small SPI  | Testing   |

## 12.12 Data Logging Example

```c
void log_sensor_reading(float temperature, float humidity)
{
    eos_file_t fd = eos_fs_open("/log.csv",
                                EOS_O_WRITE | EOS_O_CREATE | EOS_O_APPEND);
    if (fd == EOS_FILE_INVALID) return;

    char line[80];
    int len = snprintf(line, sizeof(line), "%.2f,%.2f\n",
                       temperature, humidity);
    eos_fs_write(fd, line, len);
    eos_fs_close(fd);

    /* Check if log is getting too large */
    eos_fs_stat_t st;
    eos_fs_stat(&st);
    if (st.free_bytes < 4096) {
        /* Rotate: delete old log, keep recent */
        eos_fs_remove("/log.csv.old");
        eos_fs_rename("/log.csv", "/log.csv.old");
    }
}
```

## 12.13 API Reference Summary

| Function          | Description                          |
|-------------------|--------------------------------------|
| eos_fs_init       | Initialize filesystem                |
| eos_fs_deinit     | Unmount and release resources        |
| eos_fs_format     | Format the filesystem                |
| eos_fs_stat       | Get capacity/usage statistics        |
| eos_fs_open       | Open a file with flags               |
| eos_fs_close      | Close a file descriptor              |
| eos_fs_read       | Read data from file                  |
| eos_fs_write      | Write data to file                   |
| eos_fs_seek       | Seek to position in file             |
| eos_fs_tell       | Get current file position            |
| eos_fs_truncate   | Set file size                        |
| eos_fs_sync       | Flush data to storage                |
| eos_fs_mkdir      | Create a directory                   |
| eos_fs_opendir    | Open directory for listing           |
| eos_fs_readdir    | Read next directory entry            |
| eos_fs_closedir   | Close directory handle               |
| eos_fs_remove     | Delete a file or empty directory     |
| eos_fs_rename     | Rename or move a file                |
| eos_fs_exists     | Check if a path exists               |

---

*Next: [Chapter 13 -- OTA Updates](ch13-ota.md)*
