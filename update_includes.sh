#!/bin/bash

# 更新include路径的脚本

echo "开始更新include路径..."

# 更新src目录下的文件
find src -name "*.cpp" -o -name "*.h" -o -name "*.hpp" | while read file; do
    echo "处理文件: $file"
    
    # 替换include路径
    sed -i.bak \
        -e 's|#include "apollo/aoi.hpp|#include "apollo/game/aoi/aoi.hpp|g' \
        -e 's|#include "apollo/attribute.hpp|#include "apollo/game/attributes/attribute.hpp|g' \
        -e 's|#include "apollo/logger.hpp|#include "apollo/utils/logging/logger.hpp|g' \
        -e 's|#include "apollo/thread_pool.hpp|#include "apollo/utils/threading/thread_pool.hpp|g' \
        -e 's|#include "apollo/memory_pool.hpp|#include "apollo/utils/memory/memory_pool.hpp|g' \
        -e 's|#include "apollo/db/connection.hpp|#include "apollo/storage/database/connection.hpp|g' \
        -e 's|#include "apollo/db/redis.hpp|#include "apollo/storage/cache/redis.hpp|g' \
        "$file"
    
    # 删除备份文件
    rm -f "$file.bak"
done

# 更新examples目录下的文件
find examples -name "*.cpp" -o -name "*.h" -o -name "*.hpp" | while read file; do
    echo "处理文件: $file"
    
    sed -i.bak \
        -e 's|#include "apollo/IComponent.h|#include "apollo/framework/ioc/IComponent.h|g' \
        -e 's|#include "apollo/ApplicationContext.h|#include "apollo/framework/ioc/ApplicationContext.h|g' \
        -e 's|#include "apollo/logger.hpp|#include "apollo/utils/logging/logger.hpp|g' \
        -e 's|#include "apollo/thread_pool.hpp|#include "apollo/utils/threading/thread_pool.hpp|g' \
        -e 's|#include "apollo/attribute.hpp|#include "apollo/game/attributes/attribute.hpp|g' \
        -e 's|#include "apollo/aoi.hpp|#include "apollo/game/aoi/aoi.hpp|g' \
        "$file"
    
    rm -f "$file.bak"
done

echo "Include路径更新完成！"
