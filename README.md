[![Build Status](https://github.com/12CrazyPaul21/loongdiff-win/actions/workflows/build-and-test-ldiff.yml/badge.svg)](https://github.com/12CrazyPaul21/loongdiff-win/actions) [![MIT/Apache-2 licensed](https://img.shields.io/crates/l/lopxy.svg)](./LICENSE)

# LoongDiff

<p align="center"><img src="./terminalizer/diff.gif"/></p>

<p align="center"><img src="./terminalizer/apply.gif"/></p>

ldiff 用于给二进制文件或文件夹做 diff & patch，内部依赖 [xdleta](https://github.com/jmacd/xdelta.git) 工具，可以看作是对 xdelta 的封装，xdelta 生成的补丁兼容 [VCDIFF/RFC 3284](https://www.ietf.org/rfc/rfc3284.txt) 格式，不过 ldiff 输出的补丁是用 7zip 对补丁集合的打包。

ldiff 能输出两种形式的补丁：

1. `.lpatch` 补丁文件
2. `.exe` 将补丁文件嵌入到 PE 文件内，这种补丁在执行 patch 的时候不需要 ldiff 工具，将其放入源文件目录下可独立执行，直接进行 patch

## 注意事项

- 目前只支持 Windows
- 编译环境 C++20**⤴**
- 输出 `.exe` 独立补丁时有一个限制，如果补丁包加上 `ldiff.exe` 本身的大小超过 `3.8GB` 的话，那么会自动改为输出 `.lpatch`，这是 Windows PE 格式上的限制，节大小字段是一个 32 位的整型，所以不能超过大概 `4GB`
- 执行 diff & patch 前，尽量确保文件不被其它应用所占用，ldiff 执行过程也最好别强行退出
- 执行 patch 前最好按照提示执行**备份**，或者自己手动提前备份，**避免数据丢失**
- 执行 patch 成功后，重复执行 patch 不会造成负面影响，但是最好不要这么做
- ldiff 不做并发处理，diff & patch 阶段的实际开销也依赖 xdleta 以及文件的具体大小，所以性能无法保障

## 使用方法

### diff 子命令

```bash
Usage: ldiff diff [OPTIONS] source target patch

Positionals:
  source REQUIRED             源文件或源文件夹
  target REQUIRED             目标文件或目标文件夹
  patch REQUIRED              指定补丁文件路径，不需要指定后缀名

Options:
  -h,--help                   Print this help message and exit
  -k,--keep                   保留临时文件
  -e,--output-execute         输出可独立执行patch的PE文件
  -f,--force                  如果指定的补丁文件已存在，那么强制覆写
```

### patch 子命令

```bash
Usage: ldiff patch [OPTIONS] source patch

Positionals:
  source REQUIRED             源文件或源文件夹
  patch REQUIRED              补丁文件路径

Options:
  -h,--help                   Print this help message and exit
  -k,--keep                   保留临时文件
  -i,--ignore-backup          忽略备份操作
  --backup                    指定备份文件路径（可不使用这个option,ldiff执行时会提示）
```

### 独立 PE 补丁命令

```bash
Usage: ldiff [OPTIONS] [source]

Positionals:
  source [.]                  源文件或源文件夹（可选），如果不指定则选择为当前工作目录

Options:
  -h,--help                   Print this help message and exit
  -k,--keep                   保留临时文件
  -v,--version                Display program version information and exit
  -i,--ignore-backup          忽略备份操作
  --backup                    指定备份文件路径（可不使用这个option,ldiff执行时会提示）
```

## 编译

```bash
meson setup build/release --backend=vs
# 如果backend使用ninja，为了避免编译时依赖缺失最好加上-j1
meson compile -C build/release
```

## 测试

```bash
meson test -C .\build\release --repeat 2
```

