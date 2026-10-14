# lua_precompile 冒烟（scripting-lua §3.5）：工具跑通且 manifest 行格式自洽
# （单行四 TAB 字段：name + 16 位 hex 源 hash + 16 位 hex 产物 hash + 非空签名）。
# 这是工具↔运行期 manifest 契约的最小闸——行格式漂移在此即刻红。

if(NOT exe)
    message(FATAL_ERROR "exe not set")
endif()

set(dir "${CMAKE_CURRENT_BINARY_DIR}/lua_precompile_smoke")
file(REMOVE_RECURSE "${dir}")
file(MAKE_DIRECTORY "${dir}")
file(WRITE "${dir}/hello.lua" "return 1\n")

execute_process(COMMAND "${exe}" "${dir}" "${dir}/hello.lua"
                RESULT_VARIABLE rc
                OUTPUT_VARIABLE out
                ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "lua_precompile exited ${rc}: ${out}${err}")
endif()

file(READ "${dir}/manifest" m)
# 单行四字段（TAB 分隔 + 结尾恰好一个 \n）。CMake regex 不支持 {n} 区间
# 重复——两 hash 字段的 16 位 hex 形态用显式检查兜住。
if(NOT m MATCHES "^[^\t]+\t[^\t]+\t[^\t]+\t[^\t\r\n]+\n$")
    message(FATAL_ERROR "manifest format unexpected: '${m}'")
endif()
string(REPLACE "\t" ";" f "${m}")
list(LENGTH f nf)
if(NOT nf EQUAL 4)
    message(FATAL_ERROR "manifest must have 4 TAB-separated fields, got ${nf}: '${m}'")
endif()
foreach(idx 1 2)
    list(GET f ${idx} v)
    string(LENGTH "${v}" vl)
    if(NOT vl EQUAL 16 OR NOT v MATCHES "^[0-9a-f]+$")
        message(FATAL_ERROR "field ${idx} not 16 lowercase hex chars: '${v}'")
    endif()
endforeach()

file(REMOVE_RECURSE "${dir}")
