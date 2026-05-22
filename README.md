# config_manager

**纯C多平台配置管理库** — 统一API读写 JSON、YAML、XML、INI、TOML、ENV、.properties 七种格式。

---

## 特性

| 格式 | 解析库 | 读 | 写 |
|------|--------|----|----|
| JSON | [cJSON](https://github.com/DaveGamble/cJSON) v1.7.18 | ✅ | ✅ |
| YAML | [libyaml](https://github.com/yaml/libyaml) 0.2.5 | ✅ | ✅ |
| XML  | [Mini-XML](https://github.com/michaelrsweet/mxml) v4.0.3 | ✅ | ✅ |
| INI  | [inih](https://github.com/benhoyt/inih) r58 | ✅ | ✅ |
| TOML | [tomlc99](https://github.com/cktan/tomlc99) | ✅ | ✅ |
| ENV  | 内置 (零依赖) | ✅ | ✅ |
| .properties | 内置 (零依赖) | ✅ | ✅ |

- **统一树形数据模型**：`cm_node_t` 支持 null / bool / int / float / string / array / object
- **点分路径访问**：`"server.host"`、`"list[0].name"` 直接读写嵌套字段
- **自动格式检测**：按文件扩展名自动选择解析器
- **格式互转**：JSON → YAML → TOML → INI 等任意互转
- **分层合并**：`cm_merge()` 实现 defaults → file → env → CLI 覆盖链
- **热重载**：`config_watcher` 示例展示 mtime 轮询跨平台热重载
- **多平台**：Linux / macOS / Windows（MSVC & MinGW），CMake FetchContent 自动拉取依赖

---

## 快速开始

### 构建

```bash
git clone https://github.com/yourname/config_manager.git
cd config_manager
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

运行测试：

```bash
cd build && ctest --output-on-failure
```

运行示例：

```bash
./build/examples/basic_usage
./build/examples/multi_format
./build/examples/server_config
./build/examples/config_merge
./build/examples/config_watcher          # demo 模式自动创建临时文件
./build/examples/config_watcher my.json  # 监听指定文件
```

### CMake 子项目集成

```cmake
add_subdirectory(config_manager)
target_link_libraries(your_target PRIVATE config_manager)
```

---

## API 手册

### 上下文管理

```c
cm_ctx_t *cm_ctx_create(void);           // 创建上下文
void      cm_ctx_destroy(cm_ctx_t *ctx); // 销毁并释放
void      cm_ctx_clear(cm_ctx_t *ctx);   // 清空所有数据（保留上下文）
```

### 加载 / 保存

```c
// 从文件加载，fmt=CM_FORMAT_AUTO 自动按扩展名检测
cm_error_t  cm_load_file(cm_ctx_t *ctx, const char *path, cm_format_t fmt);

// 从内存字符串加载
cm_error_t  cm_load_string(cm_ctx_t *ctx, const char *data, cm_format_t fmt);

// 保存到文件
cm_error_t  cm_save_file(cm_ctx_t *ctx, const char *path, cm_format_t fmt);

// 序列化为字符串（调用者负责 free()）
char       *cm_save_string(cm_ctx_t *ctx, cm_format_t fmt, size_t *out_len);
```

支持的 `cm_format_t` 值：

```c
CM_FORMAT_AUTO        // 按文件扩展名自动检测
CM_FORMAT_JSON
CM_FORMAT_YAML
CM_FORMAT_XML
CM_FORMAT_INI
CM_FORMAT_TOML
CM_FORMAT_ENV
CM_FORMAT_PROPERTIES
```

### 读取值

```c
// 返回 CM_OK 或错误码
cm_error_t cm_get_string(cm_ctx_t *ctx, const char *key, const char **out);
cm_error_t cm_get_int   (cm_ctx_t *ctx, const char *key, int64_t *out);
cm_error_t cm_get_float (cm_ctx_t *ctx, const char *key, double  *out);
cm_error_t cm_get_bool  (cm_ctx_t *ctx, const char *key, int     *out);

// 带默认值的版本（找不到时返回 def）
const char *cm_get_string_or(cm_ctx_t *ctx, const char *key, const char *def);
int64_t     cm_get_int_or   (cm_ctx_t *ctx, const char *key, int64_t def);
double      cm_get_float_or (cm_ctx_t *ctx, const char *key, double  def);
int         cm_get_bool_or  (cm_ctx_t *ctx, const char *key, int     def);
```

**类型自动转换**：字符串 `"42"` 可被 `cm_get_int()` 读取；`"true"/"yes"/"on"` 可被 `cm_get_bool()` 读取。

### 写入值

```c
cm_error_t cm_set_string(cm_ctx_t *ctx, const char *key, const char *val);
cm_error_t cm_set_int   (cm_ctx_t *ctx, const char *key, int64_t val);
cm_error_t cm_set_float (cm_ctx_t *ctx, const char *key, double  val);
cm_error_t cm_set_bool  (cm_ctx_t *ctx, const char *key, int     val);
cm_error_t cm_set_null  (cm_ctx_t *ctx, const char *key);
```

不存在的中间节点会自动创建：`cm_set_string(ctx, "a.b.c.d", "x")` 会自动建立 `a → b → c → d`。

### 删除 / 查询

```c
cm_error_t cm_delete (cm_ctx_t *ctx, const char *key);  // 删除键
int        cm_has_key(cm_ctx_t *ctx, const char *key);  // 1=存在 0=不存在
```

### 数组操作

```c
cm_error_t  cm_array_length    (cm_ctx_t *ctx, const char *key, size_t *out);
cm_node_t  *cm_array_get       (cm_ctx_t *ctx, const char *key, size_t index);
cm_error_t  cm_array_push_string(cm_ctx_t *ctx, const char *key, const char *val);
cm_error_t  cm_array_push_int  (cm_ctx_t *ctx, const char *key, int64_t val);
```

键路径支持下标：`"plugins[0].name"`。

### 合并

```c
// 将 src 合并进 dst
// overwrite=1：src 中的值覆盖 dst 中已有的值
// overwrite=0：dst 中已有的值保持不变，只新增 src 中 dst 没有的键
cm_error_t cm_merge(cm_ctx_t *dst, const cm_ctx_t *src, int overwrite);
```

### 遍历

```c
typedef void (*cm_walk_fn)(const char *path, cm_node_t *node, void *userdata);
void cm_walk(cm_ctx_t *ctx, cm_walk_fn fn, void *userdata);
```

遍历所有叶节点，`path` 为完整点分路径（如 `"server.host"`、`"tags[1]"`）。

### 节点树（手动构建）

```c
cm_node_t *cm_node_new_string(const char *key, const char *val);
cm_node_t *cm_node_new_int   (const char *key, int64_t val);
cm_node_t *cm_node_new_float (const char *key, double val);
cm_node_t *cm_node_new_bool  (const char *key, int val);
cm_node_t *cm_node_new_null  (const char *key);
cm_node_t *cm_node_new_object(const char *key);
cm_node_t *cm_node_new_array (const char *key);
void       cm_node_free      (cm_node_t *node);
cm_error_t cm_node_object_add(cm_node_t *obj, cm_node_t *child);
cm_error_t cm_node_array_add (cm_node_t *arr, cm_node_t *item);
```

---

## 使用示例

### 基本读写

```c
cm_ctx_t *cfg = cm_ctx_create();

// 写入
cm_set_string(cfg, "server.host",    "0.0.0.0");
cm_set_int   (cfg, "server.port",    8443);
cm_set_bool  (cfg, "server.ssl",     1);
cm_set_float (cfg, "server.timeout", 30.5);

// 读取
const char *host = cm_get_string_or(cfg, "server.host", "127.0.0.1");
int64_t     port = cm_get_int_or   (cfg, "server.port", 8080);

// 保存 JSON
cm_save_file(cfg, "config.json", CM_FORMAT_JSON);

cm_ctx_destroy(cfg);
```

### 加载任意格式

```c
cm_ctx_t *cfg = cm_ctx_create();

// 自动按扩展名检测格式
cm_load_file(cfg, "config.toml",  CM_FORMAT_AUTO);
cm_load_file(cfg, "config.yaml",  CM_FORMAT_AUTO);
cm_load_file(cfg, ".env",         CM_FORMAT_ENV);

const char *name = cm_get_string_or(cfg, "app.name", "default");
cm_ctx_destroy(cfg);
```

### 格式转换

```c
cm_ctx_t *ctx = cm_ctx_create();
cm_load_file(ctx, "config.json", CM_FORMAT_AUTO);

// 转成 YAML
cm_save_file(ctx, "config.yaml", CM_FORMAT_YAML);

// 转成 TOML
cm_save_file(ctx, "config.toml", CM_FORMAT_TOML);

cm_ctx_destroy(ctx);
```

### 分层配置（默认值 + 文件 + 环境变量 + CLI）

```c
cm_ctx_t *cfg = cm_ctx_create();

// 1. 硬编码默认值
cm_set_string(cfg, "server.host", "127.0.0.1");
cm_set_int   (cfg, "server.port", 8080);

// 2. 从文件覆盖
cm_ctx_t *file_cfg = cm_ctx_create();
cm_load_file(file_cfg, "config.yaml", CM_FORMAT_AUTO);
cm_merge(cfg, file_cfg, /*overwrite=*/1);
cm_ctx_destroy(file_cfg);

// 3. 从 .env 文件覆盖
cm_ctx_t *env_cfg = cm_ctx_create();
cm_load_file(env_cfg, ".env", CM_FORMAT_ENV);
// 手动映射 ENV_KEY → config.path
const char *p = cm_get_string_or(env_cfg, "SERVER_HOST", NULL);
if (p) cm_set_string(cfg, "server.host", p);
cm_ctx_destroy(env_cfg);

// 最终读取
printf("host=%s port=%lld\n",
    cm_get_string_or(cfg, "server.host", "?"),
    (long long)cm_get_int_or(cfg, "server.port", 0));

cm_ctx_destroy(cfg);
```

### 数组操作

```c
cm_ctx_t *cfg = cm_ctx_create();

cm_array_push_string(cfg, "server.cors_origins", "https://app.example.com");
cm_array_push_string(cfg, "server.cors_origins", "http://localhost:3000");

size_t len = 0;
cm_array_length(cfg, "server.cors_origins", &len);   // 2

cm_node_t *first = cm_array_get(cfg, "server.cors_origins", 0);
printf("%s\n", first->value.sval);

cm_ctx_destroy(cfg);
```

---

## 错误处理

所有返回 `cm_error_t` 的函数：

| 代码 | 含义 |
|------|------|
| `CM_OK` | 成功 |
| `CM_ERR_NULL_PTR` | 空指针参数 |
| `CM_ERR_NO_MEMORY` | 内存不足 |
| `CM_ERR_NOT_FOUND` | 键不存在 |
| `CM_ERR_TYPE_MISMATCH` | 类型不匹配且无法转换 |
| `CM_ERR_PARSE` | 格式解析失败 |
| `CM_ERR_IO` | 文件读写失败 |
| `CM_ERR_UNSUPPORTED` | 不支持该格式 |

```c
cm_error_t err = cm_load_file(ctx, "config.json", CM_FORMAT_AUTO);
if (err != CM_OK) {
    fprintf(stderr, "加载失败: %s\n", cm_error_str(err));
}
```

---

## CMake 选项

| 选项 | 默认 | 说明 |
|------|------|------|
| `CM_BUILD_TESTS` | ON | 构建测试 |
| `CM_BUILD_EXAMPLES` | ON | 构建示例 |
| `CM_ENABLE_JSON` | ON | 启用 JSON 支持 |
| `CM_ENABLE_YAML` | ON | 启用 YAML 支持 |
| `CM_ENABLE_XML` | ON | 启用 XML 支持 |
| `CM_ENABLE_INI` | ON | 启用 INI 支持 |
| `CM_ENABLE_TOML` | ON | 启用 TOML 支持 |
| `CM_ENABLE_ENV` | ON | 启用 ENV/.properties 支持 |

关闭不需要的格式可减少编译体积：

```bash
cmake -B build \
  -DCM_ENABLE_YAML=OFF \
  -DCM_ENABLE_XML=OFF \
  -DCM_BUILD_TESTS=OFF
```

---

## 平台支持

| 平台 | 编译器 | 状态 |
|------|--------|------|
| Linux (Ubuntu 22.04+) | GCC 11+, Clang 14+ | ✅ |
| macOS 13+ | Apple Clang, Homebrew GCC | ✅ |
| Windows 10/11 | MSVC 2019+, MinGW-w64 | ✅ |
| FreeBSD | Clang | ✅ |
| Embedded (bare-metal) | 不支持（需要 stdio/malloc） | ❌ |

---

## 目录结构

```
config_manager/
├── CMakeLists.txt          # 顶层构建
├── cmake/
│   ├── FindLibYAML.cmake   # 系统 libyaml 查找器
│   ├── FindMXML.cmake      # 系统 mxml 查找器
│   └── CompilerWarnings.cmake
├── include/
│   ├── config_manager.h    # 公共 API
│   └── cm_internal.h       # 内部声明
├── src/
│   ├── cm_core.c           # 上下文、节点树、get/set/delete/walk/merge
│   ├── cm_dispatch.c       # 格式分派（load_file / save_file）
│   ├── cm_json.c           # JSON (cJSON)
│   ├── cm_yaml.c           # YAML (libyaml)
│   ├── cm_xml.c            # XML (mxml)
│   ├── cm_ini.c            # INI (inih)
│   ├── cm_toml.c           # TOML (tomlc99)
│   └── cm_env.c            # ENV + .properties (内置)
├── tests/
│   ├── test_core.c
│   ├── test_json.c
│   ├── test_yaml.c
│   ├── test_xml.c
│   ├── test_ini.c
│   ├── test_toml.c
│   ├── test_env.c
│   └── test_merge.c        # 合并、格式互转、遍历
└── examples/
    ├── basic_usage.c       # 入门示例
    ├── server_config.c     # 服务器配置实战
    ├── multi_format.c      # 多格式等价验证
    ├── config_merge.c      # 分层合并
    └── config_watcher.c    # 文件热重载（跨平台）
```

---

## License

MIT
