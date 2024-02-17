// xdelta3-3.0.11-x86_64.exe
//  Marcos:
//      XD3_USE_LARGESIZET = 1
//      SIZEOF_USIZE_T = 8（字节）

// VCDIFF RFC3284: https://www.ietf.org/rfc/rfc3284.txt
// open-vcdiff: https://github.com/google/open-vcdiff
// xd3_decode_input: https://github.com/jmacd/xdelta/blob/release3_1_apl/xdelta3/xdelta3-decode.h

// !!!!!!!
// 只记录 xdelta3-3-3.0.11 以下参数输出的相关 patch 文件格式
// xdelta3 -s <source> <target> <patch>
// !!!!!!!

// 判断原文件与目标文件比较得出的patch<是否有必要>的依据（仅从该项目应用目标出发，不代表 VCDIFF 的实际情况）：
// !!!!!!!
// 1. 文件大小不一样保留 patch
// 2. 所有 Window 的 Data Section Length 字段都为0，可认为它们内容相同，那么不保留 patch
// 3. 如果文件大小小于 4 字节，即使它们相同，生成的 patch 的 Data Section Length 并不是 0
// !!!!!!!
// 如果只有一个 Window 并且该 Data Section Length 和 Target Window Length 都为0，表示
// 目标文件是空的
// !!!!!!!

// xdelta3-3-3.0.11 VCDIFF 基本结构（注意：还是在上面提到的特定参数输出的情况）
// - Header
//  - [4B] VCDIFF Magic [0xD6 0xC3 0xC4 0x00]
//  - [1B] Header Indicator [0x05(VCD_DECOMPRESS | VCD_APPHEADER)] 
//          前三位掩码组合指示头部剩下的部分包含哪些内容
//              bit 0(VCD_DECOMPRESS)：secondary compressor
//              bit 1(VCD_CODETABLE)：code table data
//              bit 2(VCD_APPHEADER)：application data
//          If both bits are set, then the compressor ID byte is included before
//          the code table data length and the code table data.
//          当前模式固定为0x05(0b101) -> VCD_DECOMPRESS | VCD_APPHEADER
//          也就是头部内容包括了一个 compressor id 和 application 附加data
//          这里不指定 VCD_CODETABLE 那么 xdelta3 会使用默认的 Code Table
//          	/* Use the default table. */
//              stream->acache.s_near = __rfc3284_code_table_desc.near_modes;
//              stream->acache.s_same = __rfc3284_code_table_desc.same_modes;
//              stream->code_table    = xd3_rfc3284_code_table ();
//              xd3_alloc_cache (stream);
//  - [1B] Compressor ID [0x02(VCD_LZMA_ID)]
//  - [1B ~ 8B] Application Data Length [变长编码整型]
//  - [Application Data Length] Application Data
//          这部分应该属于 xdelta3 自定义的内容，而不是 VCDIFF 协议的一部分
//          Application Data 结束后的下一个字节开始都属于 Window Data
// - Windows（数据窗口集合）
//  - Window 1
//    - [1B] Window Indicator
//          前三位掩码指示窗口类型
//              bit 0(VCD_SOURCE)：copy window in source file
//              bit 1(VCD_TARGET)：copy window in target file
//              bit 2(VCD_ADLER32)：has adler32 checksum
//          *** 如果 VCD_SOURCE 或 VCD_TARGET被设置的话就是一个 COPY Window
//    - [1B ~ 8B] Copy Window Length [非 COPY Window 不存在]
//    - [1B ~ 8B] Copy Window Offset [非 COPY Window 不存在]
//    - [1B ~ 8B] Delta Encoding Length 该窗口剩下数据的长度，用于判断是否有多个Window [*** Jackpot ***]
//    - [1B ~ 8B] Target Window Length
//    - [1B] Delta Indicator
//    - [1B ~ 8B] Data Section Length [*** Jackpot ***]
//  - Window 2
//    ...
//  - Window ...


#include <std/io.pat>
#include <std/string.pat>

#define VCD_SOURCE 1 << 0
#define VCD_TARGET 1 << 1

#define UINT64_OFLOW_MASK 0xfe00000000000000
#define SIZEOF_USIZE_T 0x08

struct DecodeSizeResult {
    u64 size;
    u64 offset;
};

// 解码变长64位无符号整数
fn decode_size(u64 _next_idx) {
    DecodeSizeResult _result;
    u8 _size_buck[SIZEOF_USIZE_T] @ _next_idx;

    while (_result.offset < SIZEOF_USIZE_T) {
        
        u8 _next_byte = _size_buck[_result.offset];
        _result.offset = _result.offset + 1;       
       
        if (_result.size & UINT64_OFLOW_MASK) {
            std::error("overflow in decode_integer");
            // abort...
        }
    
        _result.size = (_result.size << 7) | (_next_byte & 0x7F);
        
        if (_next_byte & 0x80 == 0) {
            break;
        }
    }

    return _result;
};

//
// Header
//

u32 VCDIFF_MAGIC @ 0x00; // 0xD6 0xC3 0xC4 0x00
u8 hdr_indicator @ 0x04; // 前三位掩码组合有效
u8 compressor_id @ 0x05; // 固定为VCD_LZMA_ID（0x2）

// length of application data
DecodeSizeResult apphdr_size = decode_size(0x06);
std::print("application data size : {}", apphdr_size.size);

// application data
u8 apphdr_size_array[apphdr_size.offset] @ 0x06;
char apphdr_data[apphdr_size.size] @ 0x06 + apphdr_size.offset;
std::print("application data : {}", std::string::to_string(apphdr_data));

// header
u64 header_size = 0x6 + apphdr_size.offset + apphdr_size.size;
//u8 header[header_size] @ 0x0;
std::print("vcdiff header size : {}", header_size);

//
// First Window
//

u64 next_addr = header_size;

u64 next_size_addr = 0;
DecodeSizeResult next_size_item;

fn NextByteAddr() {
    u64 _result = next_addr;
    next_addr = next_addr + 1;
    return _result;
};

fn NextSizeAddr() {
    next_size_item = decode_size(next_addr);

    u64 _result = next_addr;
    next_addr = next_addr + next_size_item.offset;
    
    return _result;
};

u8 first_window_indicator @ NextByteAddr();

if (first_window_indicator & (VCD_SOURCE | VCD_TARGET)) {
    // length of first window
    next_size_addr = NextSizeAddr();
    u8 copy_window_size[next_size_item.offset] @ next_size_addr;
    std::print("copy window size : {}", next_size_item.size);

    // first window offset
    next_size_addr = NextSizeAddr();
    u8 copy_window_offset[next_size_item.offset] @ next_size_addr;
    std::print("copy window offset : {}", next_size_item.size);
}

// length of the delta encoding
next_size_addr = NextSizeAddr();
u8 delta_encoding_size[next_size_item.offset] @ next_size_addr;
std::print("delta encoding size : {}", next_size_item.size);
 
// length of target window
next_size_addr = NextSizeAddr();
u8 target_window_size[next_size_item.offset] @ next_size_addr;
std::print("target window size : {}", next_size_item.size);

// delta indicator
u8 delta_indicator @ NextByteAddr();

// data section size
next_size_addr = NextSizeAddr();
u8 data_section_size[next_size_item.offset] @ next_size_addr;
std::print("data section size : {}", next_size_item.size);
