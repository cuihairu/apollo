export const SEARCH_INDEX = [
  {
    "title": "Apollo MMORPG 自研框架（建议版）",
    "headers": [
      {
        "level": 2,
        "title": "1. 目标与原则",
        "slug": "_1-目标与原则",
        "link": "#_1-目标与原则",
        "children": []
      },
      {
        "level": 2,
        "title": "2. 技术栈",
        "slug": "_2-技术栈",
        "link": "#_2-技术栈",
        "children": []
      },
      {
        "level": 2,
        "title": "3. 推荐架构",
        "slug": "_3-推荐架构",
        "link": "#_3-推荐架构",
        "children": []
      },
      {
        "level": 2,
        "title": "4. 核心模块职责",
        "slug": "_4-核心模块职责",
        "link": "#_4-核心模块职责",
        "children": []
      },
      {
        "level": 2,
        "title": "5. 缺失文档 & 下一步",
        "slug": "_5-缺失文档-下一步",
        "link": "#_5-缺失文档-下一步",
        "children": []
      }
    ],
    "path": "/00-Recommended_Framework.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "MMORPG服务器通信架构设计文档",
    "headers": [
      {
        "level": 2,
        "title": "目录",
        "slug": "目录",
        "link": "#目录",
        "children": []
      },
      {
        "level": 2,
        "title": "1. 整体架构概述",
        "slug": "_1-整体架构概述",
        "link": "#_1-整体架构概述",
        "children": [
          {
            "level": 3,
            "title": "1.1 架构图",
            "slug": "_1-1-架构图",
            "link": "#_1-1-架构图",
            "children": []
          },
          {
            "level": 3,
            "title": "1.2 技术栈",
            "slug": "_1-2-技术栈",
            "link": "#_1-2-技术栈",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "2. 网络通信层",
        "slug": "_2-网络通信层",
        "link": "#_2-网络通信层",
        "children": [
          {
            "level": 3,
            "title": "2.1 IOCP网络模型",
            "slug": "_2-1-iocp网络模型",
            "link": "#_2-1-iocp网络模型",
            "children": []
          },
          {
            "level": 3,
            "title": "2.2 网关会话管理",
            "slug": "_2-2-网关会话管理",
            "link": "#_2-2-网关会话管理",
            "children": []
          },
          {
            "level": 3,
            "title": "2.3 负载均衡策略",
            "slug": "_2-3-负载均衡策略",
            "link": "#_2-3-负载均衡策略",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "3. Protobuf消息系统",
        "slug": "_3-protobuf消息系统",
        "link": "#_3-protobuf消息系统",
        "children": [
          {
            "level": 3,
            "title": "3.1 消息定义结构",
            "slug": "_3-1-消息定义结构",
            "link": "#_3-1-消息定义结构",
            "children": []
          },
          {
            "level": 3,
            "title": "3.2 消息序列化/反序列化",
            "slug": "_3-2-消息序列化-反序列化",
            "link": "#_3-2-消息序列化-反序列化",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "4. 消息自动注册机制",
        "slug": "_4-消息自动注册机制",
        "link": "#_4-消息自动注册机制",
        "children": [
          {
            "level": 3,
            "title": "4.1 注册机制设计",
            "slug": "_4-1-注册机制设计",
            "link": "#_4-1-注册机制设计",
            "children": []
          },
          {
            "level": 3,
            "title": "4.2 反射机制实现",
            "slug": "_4-2-反射机制实现",
            "link": "#_4-2-反射机制实现",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "5. 数据广播系统",
        "slug": "_5-数据广播系统",
        "link": "#_5-数据广播系统",
        "children": [
          {
            "level": 3,
            "title": "5.1 广播系统架构",
            "slug": "_5-1-广播系统架构",
            "link": "#_5-1-广播系统架构",
            "children": []
          },
          {
            "level": 3,
            "title": "5.2 广播消息类型",
            "slug": "_5-2-广播消息类型",
            "link": "#_5-2-广播消息类型",
            "children": []
          },
          {
            "level": 3,
            "title": "5.3 广播优化策略",
            "slug": "_5-3-广播优化策略",
            "link": "#_5-3-广播优化策略",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "6. 数据包去重机制",
        "slug": "_6-数据包去重机制",
        "link": "#_6-数据包去重机制",
        "children": [
          {
            "level": 3,
            "title": "6.1 序列号去重",
            "slug": "_6-1-序列号去重",
            "link": "#_6-1-序列号去重",
            "children": []
          },
          {
            "level": 3,
            "title": "6.2 消息指纹去重",
            "slug": "_6-2-消息指纹去重",
            "link": "#_6-2-消息指纹去重",
            "children": []
          },
          {
            "level": 3,
            "title": "6.3 综合去重策略",
            "slug": "_6-3-综合去重策略",
            "link": "#_6-3-综合去重策略",
            "children": []
          },
          {
            "level": 3,
            "title": "6.4 性能优化",
            "slug": "_6-4-性能优化",
            "link": "#_6-4-性能优化",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "7. 服务器分层架构",
        "slug": "_7-服务器分层架构",
        "link": "#_7-服务器分层架构",
        "children": [
          {
            "level": 3,
            "title": "7.1 微服务划分",
            "slug": "_7-1-微服务划分",
            "link": "#_7-1-微服务划分",
            "children": []
          },
          {
            "level": 3,
            "title": "7.2 服务间通信",
            "slug": "_7-2-服务间通信",
            "link": "#_7-2-服务间通信",
            "children": []
          },
          {
            "level": 3,
            "title": "7.3 服务发现机制",
            "slug": "_7-3-服务发现机制",
            "link": "#_7-3-服务发现机制",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "总结",
        "slug": "总结",
        "link": "#总结",
        "children": []
      }
    ],
    "path": "/01-%E9%80%9A%E4%BF%A1%E6%9E%B6%E6%9E%84%E8%AE%BE%E8%AE%A1.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "配置自动加载系统文档",
    "headers": [
      {
        "level": 2,
        "title": "目录",
        "slug": "目录",
        "link": "#目录",
        "children": []
      },
      {
        "level": 2,
        "title": "1. 系统概述",
        "slug": "_1-系统概述",
        "link": "#_1-系统概述",
        "children": [
          {
            "level": 3,
            "title": "1.1 设计目标",
            "slug": "_1-1-设计目标",
            "link": "#_1-1-设计目标",
            "children": []
          },
          {
            "level": 3,
            "title": "1.2 配置类型",
            "slug": "_1-2-配置类型",
            "link": "#_1-2-配置类型",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "2. 配置文件管理",
        "slug": "_2-配置文件管理",
        "link": "#_2-配置文件管理",
        "children": [
          {
            "level": 3,
            "title": "2.1 配置目录结构",
            "slug": "_2-1-配置目录结构",
            "link": "#_2-1-配置目录结构",
            "children": []
          },
          {
            "level": 3,
            "title": "2.2 配置管理器",
            "slug": "_2-2-配置管理器",
            "link": "#_2-2-配置管理器",
            "children": []
          },
          {
            "level": 3,
            "title": "2.3 配置加载器接口",
            "slug": "_2-3-配置加载器接口",
            "link": "#_2-3-配置加载器接口",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "3. Excel到Lua转换",
        "slug": "_3-excel到lua转换",
        "link": "#_3-excel到lua转换",
        "children": [
          {
            "level": 3,
            "title": "3.1 转换工具设计",
            "slug": "_3-1-转换工具设计",
            "link": "#_3-1-转换工具设计",
            "children": []
          },
          {
            "level": 3,
            "title": "3.2 Lua配置加载器",
            "slug": "_3-2-lua配置加载器",
            "link": "#_3-2-lua配置加载器",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "4. 配置热更新机制",
        "slug": "_4-配置热更新机制",
        "link": "#_4-配置热更新机制",
        "children": [
          {
            "level": 3,
            "title": "4.1 文件监控器",
            "slug": "_4-1-文件监控器",
            "link": "#_4-1-文件监控器",
            "children": []
          },
          {
            "level": 3,
            "title": "4.2 热更新控制器",
            "slug": "_4-2-热更新控制器",
            "link": "#_4-2-热更新控制器",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "5. 配置加载与校验",
        "slug": "_5-配置加载与校验",
        "link": "#_5-配置加载与校验",
        "children": [
          {
            "level": 3,
            "title": "5.1 配置校验器",
            "slug": "_5-1-配置校验器",
            "link": "#_5-1-配置校验器",
            "children": []
          },
          {
            "level": 3,
            "title": "5.2 配置依赖管理",
            "slug": "_5-2-配置依赖管理",
            "link": "#_5-2-配置依赖管理",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "6. 配置版本管理",
        "slug": "_6-配置版本管理",
        "link": "#_6-配置版本管理",
        "children": [
          {
            "level": 3,
            "title": "6.1 版本管理器",
            "slug": "_6-1-版本管理器",
            "link": "#_6-1-版本管理器",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "7. 配置缓存系统",
        "slug": "_7-配置缓存系统",
        "link": "#_7-配置缓存系统",
        "children": [
          {
            "level": 3,
            "title": "7.1 高性能缓存实现",
            "slug": "_7-1-高性能缓存实现",
            "link": "#_7-1-高性能缓存实现",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "总结",
        "slug": "总结",
        "link": "#总结",
        "children": []
      }
    ],
    "path": "/02-%E9%85%8D%E7%BD%AE%E8%87%AA%E5%8A%A8%E5%8A%A0%E8%BD%BD%E7%B3%BB%E7%BB%9F.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "MMORPG属性系统与同步机制文档",
    "headers": [
      {
        "level": 2,
        "title": "目录",
        "slug": "目录",
        "link": "#目录",
        "children": []
      },
      {
        "level": 2,
        "title": "1. 系统概述",
        "slug": "_1-系统概述",
        "link": "#_1-系统概述",
        "children": [
          {
            "level": 3,
            "title": "1.1 设计目标",
            "slug": "_1-1-设计目标",
            "link": "#_1-1-设计目标",
            "children": []
          },
          {
            "level": 3,
            "title": "1.2 属性分类",
            "slug": "_1-2-属性分类",
            "link": "#_1-2-属性分类",
            "children": []
          },
          {
            "level": 3,
            "title": "1.3 属性标记系统",
            "slug": "_1-3-属性标记系统",
            "link": "#_1-3-属性标记系统",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "2. 属性定义系统",
        "slug": "_2-属性定义系统",
        "link": "#_2-属性定义系统",
        "children": [
          {
            "level": 3,
            "title": "2.1 属性配置表",
            "slug": "_2-1-属性配置表",
            "link": "#_2-1-属性配置表",
            "children": []
          },
          {
            "level": 3,
            "title": "2.2 属性定义管理器",
            "slug": "_2-2-属性定义管理器",
            "link": "#_2-2-属性定义管理器",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "3. 属性存储结构",
        "slug": "_3-属性存储结构",
        "link": "#_3-属性存储结构",
        "children": [
          {
            "level": 3,
            "title": "3.1 属性容器设计",
            "slug": "_3-1-属性容器设计",
            "link": "#_3-1-属性容器设计",
            "children": []
          },
          {
            "level": 3,
            "title": "3.2 属性修饰器",
            "slug": "_3-2-属性修饰器",
            "link": "#_3-2-属性修饰器",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "4. 属性同步机制",
        "slug": "_4-属性同步机制",
        "link": "#_4-属性同步机制",
        "children": [
          {
            "level": 3,
            "title": "4.1 同步策略设计",
            "slug": "_4-1-同步策略设计",
            "link": "#_4-1-同步策略设计",
            "children": []
          },
          {
            "level": 3,
            "title": "4.2 差分同步优化",
            "slug": "_4-2-差分同步优化",
            "link": "#_4-2-差分同步优化",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "5. 属性变化追踪",
        "slug": "_5-属性变化追踪",
        "link": "#_5-属性变化追踪",
        "children": [
          {
            "level": 3,
            "title": "5.1 变化追踪系统",
            "slug": "_5-1-变化追踪系统",
            "link": "#_5-1-变化追踪系统",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "6. 属性广播系统",
        "slug": "_6-属性广播系统",
        "link": "#_6-属性广播系统",
        "children": [
          {
            "level": 3,
            "title": "6.1 智能广播策略",
            "slug": "_6-1-智能广播策略",
            "link": "#_6-1-智能广播策略",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "7. 属性持久化",
        "slug": "_7-属性持久化",
        "link": "#_7-属性持久化",
        "children": [
          {
            "level": 3,
            "title": "7.1 数据库存储策略",
            "slug": "_7-1-数据库存储策略",
            "link": "#_7-1-数据库存储策略",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "8. 性能优化策略",
        "slug": "_8-性能优化策略",
        "link": "#_8-性能优化策略",
        "children": [
          {
            "level": 3,
            "title": "8.1 缓存优化",
            "slug": "_8-1-缓存优化",
            "link": "#_8-1-缓存优化",
            "children": []
          },
          {
            "level": 3,
            "title": "8.2 内存优化",
            "slug": "_8-2-内存优化",
            "link": "#_8-2-内存优化",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "总结",
        "slug": "总结",
        "link": "#总结",
        "children": []
      }
    ],
    "path": "/03-%E5%B1%9E%E6%80%A7%E7%B3%BB%E7%BB%9F%E4%B8%8E%E5%90%8C%E6%AD%A5%E6%9C%BA%E5%88%B6.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "Apollo 游戏服务器框架架构设计方案",
    "headers": [
      {
        "level": 2,
        "title": "目录",
        "slug": "目录",
        "link": "#目录",
        "children": []
      },
      {
        "level": 2,
        "title": "1. 项目概述",
        "slug": "_1-项目概述",
        "link": "#_1-项目概述",
        "children": [
          {
            "level": 3,
            "title": "1.1 项目目标",
            "slug": "_1-1-项目目标",
            "link": "#_1-1-项目目标",
            "children": []
          },
          {
            "level": 3,
            "title": "1.2 核心功能模块",
            "slug": "_1-2-核心功能模块",
            "link": "#_1-2-核心功能模块",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "2. 技术选型",
        "slug": "_2-技术选型",
        "link": "#_2-技术选型",
        "children": [
          {
            "level": 3,
            "title": "2.1 编程语言与标准",
            "slug": "_2-1-编程语言与标准",
            "link": "#_2-1-编程语言与标准",
            "children": []
          },
          {
            "level": 3,
            "title": "2.2 第三方依赖",
            "slug": "_2-2-第三方依赖",
            "link": "#_2-2-第三方依赖",
            "children": []
          },
          {
            "level": 3,
            "title": "2.4 平台支持矩阵",
            "slug": "_2-4-平台支持矩阵",
            "link": "#_2-4-平台支持矩阵",
            "children": []
          },
          {
            "level": 3,
            "title": "2.5 异步 I/O 模型选择",
            "slug": "_2-5-异步-i-o-模型选择",
            "link": "#_2-5-异步-i-o-模型选择",
            "children": []
          },
          {
            "level": 3,
            "title": "2.6 服务器标识与进程间通信",
            "slug": "_2-6-服务器标识与进程间通信",
            "link": "#_2-6-服务器标识与进程间通信",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "3. 目录结构",
        "slug": "_3-目录结构",
        "link": "#_3-目录结构",
        "children": []
      },
      {
        "level": 2,
        "title": "4. 基础设施层设计",
        "slug": "_4-基础设施层设计",
        "link": "#_4-基础设施层设计",
        "children": [
          {
            "level": 3,
            "title": "4.1 网络模块 (Network)",
            "slug": "_4-1-网络模块-network",
            "link": "#_4-1-网络模块-network",
            "children": []
          },
          {
            "level": 3,
            "title": "4.2 配置模块 (Config)",
            "slug": "_4-2-配置模块-config",
            "link": "#_4-2-配置模块-config",
            "children": []
          },
          {
            "level": 3,
            "title": "4.3 属性模块 (Property)",
            "slug": "_4-3-属性模块-property",
            "link": "#_4-3-属性模块-property",
            "children": []
          },
          {
            "level": 3,
            "title": "4.4 数据库模块 (Database)",
            "slug": "_4-4-数据库模块-database",
            "link": "#_4-4-数据库模块-database",
            "children": []
          },
          {
            "level": 3,
            "title": "4.5 通用模块 (Common)",
            "slug": "_4-5-通用模块-common",
            "link": "#_4-5-通用模块-common",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "5. 构建系统设计",
        "slug": "_5-构建系统设计",
        "link": "#_5-构建系统设计",
        "children": [
          {
            "level": 3,
            "title": "5.1 CMake 配置",
            "slug": "_5-1-cmake-配置",
            "link": "#_5-1-cmake-配置",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "6. 依赖管理",
        "slug": "_6-依赖管理",
        "link": "#_6-依赖管理",
        "children": [
          {
            "level": 3,
            "title": "6.1 vcpkg 配置",
            "slug": "_6-1-vcpkg-配置",
            "link": "#_6-1-vcpkg-配置",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "7. 开发规范",
        "slug": "_7-开发规范",
        "link": "#_7-开发规范",
        "children": [
          {
            "level": 3,
            "title": "7.1 代码风格",
            "slug": "_7-1-代码风格",
            "link": "#_7-1-代码风格",
            "children": []
          },
          {
            "level": 3,
            "title": "7.2 命名规范",
            "slug": "_7-2-命名规范",
            "link": "#_7-2-命名规范",
            "children": []
          },
          {
            "level": 3,
            "title": "7.3 目录/模块规范",
            "slug": "_7-3-目录-模块规范",
            "link": "#_7-3-目录-模块规范",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "8. 实施计划",
        "slug": "_8-实施计划",
        "link": "#_8-实施计划",
        "children": [
          {
            "level": 3,
            "title": "8.1 阶段划分",
            "slug": "_8-1-阶段划分",
            "link": "#_8-1-阶段划分",
            "children": []
          },
          {
            "level": 3,
            "title": "8.2 里程碑",
            "slug": "_8-2-里程碑",
            "link": "#_8-2-里程碑",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "总结",
        "slug": "总结",
        "link": "#总结",
        "children": []
      }
    ],
    "path": "/04-%E6%A1%86%E6%9E%B6%E6%9E%B6%E6%9E%84%E8%AE%BE%E8%AE%A1%E6%96%B9%E6%A1%88.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "MMORPG 服务器架构设计方案 (Codex 审核版)",
    "headers": [
      {
        "level": 2,
        "title": "目录",
        "slug": "目录",
        "link": "#目录",
        "children": []
      },
      {
        "level": 2,
        "title": "1. 架构概述",
        "slug": "_1-架构概述",
        "link": "#_1-架构概述",
        "children": [
          {
            "level": 3,
            "title": "1.1 设计目标",
            "slug": "_1-1-设计目标",
            "link": "#_1-1-设计目标",
            "children": []
          },
          {
            "level": 3,
            "title": "1.2 架构原则",
            "slug": "_1-2-架构原则",
            "link": "#_1-2-架构原则",
            "children": []
          },
          {
            "level": 3,
            "title": "1.3 整体架构图",
            "slug": "_1-3-整体架构图",
            "link": "#_1-3-整体架构图",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "2. 服务拓扑设计",
        "slug": "_2-服务拓扑设计",
        "link": "#_2-服务拓扑设计",
        "children": [
          {
            "level": 3,
            "title": "2.1 服务清单与职责",
            "slug": "_2-1-服务清单与职责",
            "link": "#_2-1-服务清单与职责",
            "children": []
          },
          {
            "level": 3,
            "title": "2.2 ServerID 设计 (64位)",
            "slug": "_2-2-serverid-设计-64位",
            "link": "#_2-2-serverid-设计-64位",
            "children": []
          },
          {
            "level": 3,
            "title": "2.3 服务发现与注册",
            "slug": "_2-3-服务发现与注册",
            "link": "#_2-3-服务发现与注册",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "3. 核心技术决策",
        "slug": "_3-核心技术决策",
        "link": "#_3-核心技术决策",
        "children": [
          {
            "level": 3,
            "title": "3.1 技术栈选型",
            "slug": "_3-1-技术栈选型",
            "link": "#_3-1-技术栈选型",
            "children": []
          },
          {
            "level": 3,
            "title": "3.2 异步 I/O 模型选择",
            "slug": "_3-2-异步-i-o-模型选择",
            "link": "#_3-2-异步-i-o-模型选择",
            "children": []
          },
          {
            "level": 3,
            "title": "3.3 线程模型",
            "slug": "_3-3-线程模型",
            "link": "#_3-3-线程模型",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "4. 通信架构",
        "slug": "_4-通信架构",
        "link": "#_4-通信架构",
        "children": [
          {
            "level": 3,
            "title": "4.1 协议分层",
            "slug": "_4-1-协议分层",
            "link": "#_4-1-协议分层",
            "children": []
          },
          {
            "level": 3,
            "title": "4.2 消息路由",
            "slug": "_4-2-消息路由",
            "link": "#_4-2-消息路由",
            "children": []
          },
          {
            "level": 3,
            "title": "4.3 消息自动注册",
            "slug": "_4-3-消息自动注册",
            "link": "#_4-3-消息自动注册",
            "children": []
          },
          {
            "level": 3,
            "title": "4.4 数据包去重",
            "slug": "_4-4-数据包去重",
            "link": "#_4-4-数据包去重",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "5. 数据架构",
        "slug": "_5-数据架构",
        "link": "#_5-数据架构",
        "children": [
          {
            "level": 3,
            "title": "5.1 数据分层",
            "slug": "_5-1-数据分层",
            "link": "#_5-1-数据分层",
            "children": []
          },
          {
            "level": 3,
            "title": "5.2 数据库分库分表",
            "slug": "_5-2-数据库分库分表",
            "link": "#_5-2-数据库分库分表",
            "children": []
          },
          {
            "level": 3,
            "title": "5.3 数据持久化策略",
            "slug": "_5-3-数据持久化策略",
            "link": "#_5-3-数据持久化策略",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "6. 属性与同步系统",
        "slug": "_6-属性与同步系统",
        "link": "#_6-属性与同步系统",
        "children": [
          {
            "level": 3,
            "title": "6.1 属性分类",
            "slug": "_6-1-属性分类",
            "link": "#_6-1-属性分类",
            "children": []
          },
          {
            "level": 3,
            "title": "6.2 属性容器设计",
            "slug": "_6-2-属性容器设计",
            "link": "#_6-2-属性容器设计",
            "children": []
          },
          {
            "level": 3,
            "title": "6.3 同步标记系统",
            "slug": "_6-3-同步标记系统",
            "link": "#_6-3-同步标记系统",
            "children": []
          },
          {
            "level": 3,
            "title": "6.4 差分同步优化",
            "slug": "_6-4-差分同步优化",
            "link": "#_6-4-差分同步优化",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "7. 场景与 AOI 系统",
        "slug": "_7-场景与-aoi-系统",
        "link": "#_7-场景与-aoi-系统",
        "children": [
          {
            "level": 3,
            "title": "7.1 场景架构",
            "slug": "_7-1-场景架构",
            "link": "#_7-1-场景架构",
            "children": []
          },
          {
            "level": 3,
            "title": "7.2 AOI (Area of Interest) 系统",
            "slug": "_7-2-aoi-area-of-interest-系统",
            "link": "#_7-2-aoi-area-of-interest-系统",
            "children": []
          },
          {
            "level": 3,
            "title": "7.3 场景迁移",
            "slug": "_7-3-场景迁移",
            "link": "#_7-3-场景迁移",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "8. 高可用与容灾",
        "slug": "_8-高可用与容灾",
        "link": "#_8-高可用与容灾",
        "children": [
          {
            "level": 3,
            "title": "8.1 服务高可用",
            "slug": "_8-1-服务高可用",
            "link": "#_8-1-服务高可用",
            "children": []
          },
          {
            "level": 3,
            "title": "8.2 数据库容灾",
            "slug": "_8-2-数据库容灾",
            "link": "#_8-2-数据库容灾",
            "children": []
          },
          {
            "level": 3,
            "title": "8.3 灾难恢复",
            "slug": "_8-3-灾难恢复",
            "link": "#_8-3-灾难恢复",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "9. 性能指标与约束",
        "slug": "_9-性能指标与约束",
        "link": "#_9-性能指标与约束",
        "children": [
          {
            "level": 3,
            "title": "9.1 关键性能指标",
            "slug": "_9-1-关键性能指标",
            "link": "#_9-1-关键性能指标",
            "children": []
          },
          {
            "level": 3,
            "title": "9.2 资源规划",
            "slug": "_9-2-资源规划",
            "link": "#_9-2-资源规划",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "10. 实施路线图",
        "slug": "_10-实施路线图",
        "link": "#_10-实施路线图",
        "children": [
          {
            "level": 3,
            "title": "10.1 阶段划分",
            "slug": "_10-1-阶段划分",
            "link": "#_10-1-阶段划分",
            "children": []
          },
          {
            "level": 3,
            "title": "10.2 里程碑",
            "slug": "_10-2-里程碑",
            "link": "#_10-2-里程碑",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "11. 待审核问题清单",
        "slug": "_11-待审核问题清单",
        "link": "#_11-待审核问题清单",
        "children": [
          {
            "level": 3,
            "title": "11.1 架构决策",
            "slug": "_11-1-架构决策",
            "link": "#_11-1-架构决策",
            "children": []
          },
          {
            "level": 3,
            "title": "11.2 技术选型",
            "slug": "_11-2-技术选型",
            "link": "#_11-2-技术选型",
            "children": []
          },
          {
            "level": 3,
            "title": "11.3 性能约束",
            "slug": "_11-3-性能约束",
            "link": "#_11-3-性能约束",
            "children": []
          },
          {
            "level": 3,
            "title": "11.4 高可用",
            "slug": "_11-4-高可用",
            "link": "#_11-4-高可用",
            "children": []
          }
        ]
      }
    ],
    "path": "/05-MMORPG%E6%9C%8D%E5%8A%A1%E5%99%A8%E6%9E%B6%E6%9E%84%E8%AE%BE%E8%AE%A1%E6%96%B9%E6%A1%88-Codex%E5%AE%A1%E6%A0%B8%E7%89%88.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "Apollo Component Lifecycle Management Design",
    "headers": [
      {
        "level": 2,
        "title": "1. Design Philosophy",
        "slug": "_1-design-philosophy",
        "link": "#_1-design-philosophy",
        "children": []
      },
      {
        "level": 2,
        "title": "2. Lifecycle Definition",
        "slug": "_2-lifecycle-definition",
        "link": "#_2-lifecycle-definition",
        "children": []
      },
      {
        "level": 2,
        "title": "3. Bean vs Component",
        "slug": "_3-bean-vs-component",
        "link": "#_3-bean-vs-component",
        "children": []
      },
      {
        "level": 2,
        "title": "4. Implementation Design",
        "slug": "_4-implementation-design",
        "link": "#_4-implementation-design",
        "children": [
          {
            "level": 3,
            "title": "4.1 The Interface (IComponent)",
            "slug": "_4-1-the-interface-icomponent",
            "link": "#_4-1-the-interface-icomponent",
            "children": []
          },
          {
            "level": 3,
            "title": "4.2 The Manager (ApplicationContext)",
            "slug": "_4-2-the-manager-applicationcontext",
            "link": "#_4-2-the-manager-applicationcontext",
            "children": []
          },
          {
            "level": 3,
            "title": "4.3 Usage Example",
            "slug": "_4-3-usage-example",
            "link": "#_4-3-usage-example",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "5. Advanced Features (Optional)",
        "slug": "_5-advanced-features-optional",
        "link": "#_5-advanced-features-optional",
        "children": [
          {
            "level": 3,
            "title": "5.1 Explicit Dependencies (DAG)",
            "slug": "_5-1-explicit-dependencies-dag",
            "link": "#_5-1-explicit-dependencies-dag",
            "children": []
          },
          {
            "level": 3,
            "title": "5.2 Async Startup",
            "slug": "_5-2-async-startup",
            "link": "#_5-2-async-startup",
            "children": []
          },
          {
            "level": 3,
            "title": "5.3 Hot Reload Hook",
            "slug": "_5-3-hot-reload-hook",
            "link": "#_5-3-hot-reload-hook",
            "children": []
          },
          {
            "level": 3,
            "title": "5.4 Component Discovery (Service Locator)",
            "slug": "_5-4-component-discovery-service-locator",
            "link": "#_5-4-component-discovery-service-locator",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "6. Spring-like Features Implementation",
        "slug": "_6-spring-like-features-implementation",
        "link": "#_6-spring-like-features-implementation",
        "children": [
          {
            "level": 3,
            "title": "6.1 Auto-Registration (@Component)",
            "slug": "_6-1-auto-registration-component",
            "link": "#_6-1-auto-registration-component",
            "children": []
          },
          {
            "level": 3,
            "title": "6.2 Configuration Injection (@Value)",
            "slug": "_6-2-configuration-injection-value",
            "link": "#_6-2-configuration-injection-value",
            "children": []
          },
          {
            "level": 3,
            "title": "6.3 Advanced Lifecycle Control (SmartLifecycle)",
            "slug": "_6-3-advanced-lifecycle-control-smartlifecycle",
            "link": "#_6-3-advanced-lifecycle-control-smartlifecycle",
            "children": []
          },
          {
            "level": 3,
            "title": "6.4 Application Runner (SpringApplication.run)",
            "slug": "_6-4-application-runner-springapplication-run",
            "link": "#_6-4-application-runner-springapplication-run",
            "children": []
          },
          {
            "level": 3,
            "title": "6.5 Bean Concepts & Lookup Strategies",
            "slug": "_6-5-bean-concepts-lookup-strategies",
            "link": "#_6-5-bean-concepts-lookup-strategies",
            "children": []
          }
        ]
      }
    ],
    "path": "/06-%E7%94%9F%E5%91%BD%E5%91%A8%E6%9C%9F%E7%AE%A1%E7%90%86%E8%AE%BE%E8%AE%A1.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "专业 MMORPG 服务器架构设计方案 (Reference Architecture)",
    "headers": [
      {
        "level": 2,
        "title": "1. 核心架构设计理念",
        "slug": "_1-核心架构设计理念",
        "link": "#_1-核心架构设计理念",
        "children": [
          {
            "level": 3,
            "title": "1.1 设计哲学",
            "slug": "_1-1-设计哲学",
            "link": "#_1-1-设计哲学",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "2. 服务拆分与拓扑 (Service Topology)",
        "slug": "_2-服务拆分与拓扑-service-topology",
        "link": "#_2-服务拆分与拓扑-service-topology",
        "children": [
          {
            "level": 3,
            "title": "2.1 推荐的微服务拓扑",
            "slug": "_2-1-推荐的微服务拓扑",
            "link": "#_2-1-推荐的微服务拓扑",
            "children": []
          },
          {
            "level": 3,
            "title": "2.2 关键服务职责详解",
            "slug": "_2-2-关键服务职责详解",
            "link": "#_2-2-关键服务职责详解",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "3. 生命周期管理 (Lifecycle Management)",
        "slug": "_3-生命周期管理-lifecycle-management",
        "link": "#_3-生命周期管理-lifecycle-management",
        "children": [
          {
            "level": 3,
            "title": "3.1 服务器节点生命周期",
            "slug": "_3-1-服务器节点生命周期",
            "link": "#_3-1-服务器节点生命周期",
            "children": []
          },
          {
            "level": 3,
            "title": "3.2 玩家 Session 生命周期 (Player Lifecycle)",
            "slug": "_3-2-玩家-session-生命周期-player-lifecycle",
            "link": "#_3-2-玩家-session-生命周期-player-lifecycle",
            "children": []
          },
          {
            "level": 3,
            "title": "3.3 场景 生命周期 (Scene Lifecycle)",
            "slug": "_3-3-场景-生命周期-scene-lifecycle",
            "link": "#_3-3-场景-生命周期-scene-lifecycle",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "4. 战斗系统设计 (Battle Design)",
        "slug": "_4-战斗系统设计-battle-design",
        "link": "#_4-战斗系统设计-battle-design",
        "children": [
          {
            "level": 3,
            "title": "4.1 战斗实体架构 (Entity Structure)",
            "slug": "_4-1-战斗实体架构-entity-structure",
            "link": "#_4-1-战斗实体架构-entity-structure",
            "children": []
          },
          {
            "level": 3,
            "title": "4.2 技能施法流程 (The Skill Pipeline)",
            "slug": "_4-2-技能施法流程-the-skill-pipeline",
            "link": "#_4-2-技能施法流程-the-skill-pipeline",
            "children": []
          },
          {
            "level": 3,
            "title": "4.3 状态同步策略",
            "slug": "_4-3-状态同步策略",
            "link": "#_4-3-状态同步策略",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "5. 数据存储与一致性 (Persistence & Consistency)",
        "slug": "_5-数据存储与一致性-persistence-consistency",
        "link": "#_5-数据存储与一致性-persistence-consistency",
        "children": [
          {
            "level": 3,
            "title": "5.1 数据模型设计",
            "slug": "_5-1-数据模型设计",
            "link": "#_5-1-数据模型设计",
            "children": []
          },
          {
            "level": 3,
            "title": "5.2 缓存策略 (Look-aside + Write-back)",
            "slug": "_5-2-缓存策略-look-aside-write-back",
            "link": "#_5-2-缓存策略-look-aside-write-back",
            "children": []
          },
          {
            "level": 3,
            "title": "5.3 极端情况的数据保护",
            "slug": "_5-3-极端情况的数据保护",
            "link": "#_5-3-极端情况的数据保护",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "6. 总结建议",
        "slug": "_6-总结建议",
        "link": "#_6-总结建议",
        "children": []
      }
    ],
    "path": "/07-Recommended_MMORPG_Architecture.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "Apollo Framework Scope Management Design",
    "headers": [
      {
        "level": 2,
        "title": "1. Scope 概念与设计目标",
        "slug": "_1-scope-概念与设计目标",
        "link": "#_1-scope-概念与设计目标",
        "children": [
          {
            "level": 3,
            "title": "1.1 为什么需要 Scope",
            "slug": "_1-1-为什么需要-scope",
            "link": "#_1-1-为什么需要-scope",
            "children": []
          },
          {
            "level": 3,
            "title": "1.2 设计目标",
            "slug": "_1-2-设计目标",
            "link": "#_1-2-设计目标",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "2. Scope 类型定义",
        "slug": "_2-scope-类型定义",
        "link": "#_2-scope-类型定义",
        "children": [
          {
            "level": 3,
            "title": "2.1 内置 Scope 类型",
            "slug": "_2-1-内置-scope-类型",
            "link": "#_2-1-内置-scope-类型",
            "children": []
          },
          {
            "level": 3,
            "title": "2.2 Scope 生命周期对比",
            "slug": "_2-2-scope-生命周期对比",
            "link": "#_2-2-scope-生命周期对比",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "3. 核心接口设计",
        "slug": "_3-核心接口设计",
        "link": "#_3-核心接口设计",
        "children": [
          {
            "level": 3,
            "title": "3.1 Scope 接口",
            "slug": "_3-1-scope-接口",
            "link": "#_3-1-scope-接口",
            "children": []
          },
          {
            "level": 3,
            "title": "3.2 Bean 定义扩展",
            "slug": "_3-2-bean-定义扩展",
            "link": "#_3-2-bean-定义扩展",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "4. 具体实现",
        "slug": "_4-具体实现",
        "link": "#_4-具体实现",
        "children": [
          {
            "level": 3,
            "title": "4.1 Singleton Scope",
            "slug": "_4-1-singleton-scope",
            "link": "#_4-1-singleton-scope",
            "children": []
          },
          {
            "level": 3,
            "title": "4.2 Prototype Scope",
            "slug": "_4-2-prototype-scope",
            "link": "#_4-2-prototype-scope",
            "children": []
          },
          {
            "level": 3,
            "title": "4.3 Thread Scope",
            "slug": "_4-3-thread-scope",
            "link": "#_4-3-thread-scope",
            "children": []
          },
          {
            "level": 3,
            "title": "4.4 Request Scope",
            "slug": "_4-4-request-scope",
            "link": "#_4-4-request-scope",
            "children": []
          },
          {
            "level": 3,
            "title": "4.5 Session Scope",
            "slug": "_4-5-session-scope",
            "link": "#_4-5-session-scope",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "5. Scope 管理器实现",
        "slug": "_5-scope-管理器实现",
        "link": "#_5-scope-管理器实现",
        "children": []
      },
      {
        "level": 2,
        "title": "6. ApplicationContext 集成",
        "slug": "_6-applicationcontext-集成",
        "link": "#_6-applicationcontext-集成",
        "children": []
      },
      {
        "level": 2,
        "title": "7. 使用示例",
        "slug": "_7-使用示例",
        "link": "#_7-使用示例",
        "children": []
      },
      {
        "level": 2,
        "title": "8. 自定义 Scope 支持",
        "slug": "_8-自定义-scope-支持",
        "link": "#_8-自定义-scope-支持",
        "children": []
      },
      {
        "level": 2,
        "title": "9. 性能优化建议",
        "slug": "_9-性能优化建议",
        "link": "#_9-性能优化建议",
        "children": []
      },
      {
        "level": 2,
        "title": "10. 注意事项",
        "slug": "_10-注意事项",
        "link": "#_10-注意事项",
        "children": []
      }
    ],
    "path": "/07-Scope%E7%AE%A1%E7%90%86%E8%AE%BE%E8%AE%A1.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "玩家数据存储方案：性能与 BI 的平衡之道 (Hybrid Storage Design)",
    "headers": [
      {
        "level": 2,
        "title": "1. 数据库表结构设计 (Hybrid Schema)",
        "slug": "_1-数据库表结构设计-hybrid-schema",
        "link": "#_1-数据库表结构设计-hybrid-schema",
        "children": [
          {
            "level": 3,
            "title": "1.1 核心表结构 (t_character)",
            "slug": "_1-1-核心表结构-t-character",
            "link": "#_1-1-核心表结构-t-character",
            "children": []
          },
          {
            "level": 3,
            "title": "1.2 为什么这样设计？",
            "slug": "_1-2-为什么这样设计",
            "link": "#_1-2-为什么这样设计",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "2. 属性的序列化存储 (Protobuf Design)",
        "slug": "_2-属性的序列化存储-protobuf-design",
        "link": "#_2-属性的序列化存储-protobuf-design",
        "children": []
      },
      {
        "level": 2,
        "title": "3. 真正的 BI 核心：TLog (Transaction Log)",
        "slug": "_3-真正的-bi-核心-tlog-transaction-log",
        "link": "#_3-真正的-bi-核心-tlog-transaction-log",
        "children": [
          {
            "level": 3,
            "title": "3.1 埋点日志系统设计",
            "slug": "_3-1-埋点日志系统设计",
            "link": "#_3-1-埋点日志系统设计",
            "children": []
          },
          {
            "level": 3,
            "title": "3.2 BI 架构图",
            "slug": "_3-2-bi-架构图",
            "link": "#_3-2-bi-架构图",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "4. 总结",
        "slug": "_4-总结",
        "link": "#_4-总结",
        "children": []
      }
    ],
    "path": "/08-Player_Data_Storage_and_BI.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "Spring Bean 生命周期与 Scope 知识点总结",
    "headers": [
      {
        "level": 2,
        "title": "目录",
        "slug": "目录",
        "link": "#目录",
        "children": []
      },
      {
        "level": 2,
        "title": "1. Bean 的 Scope 类型",
        "slug": "_1-bean-的-scope-类型",
        "link": "#_1-bean-的-scope-类型",
        "children": [
          {
            "level": 3,
            "title": "1.1 六种内置 Scope",
            "slug": "_1-1-六种内置-scope",
            "link": "#_1-1-六种内置-scope",
            "children": []
          },
          {
            "level": 3,
            "title": "1.2 Scope 定义方式",
            "slug": "_1-2-scope-定义方式",
            "link": "#_1-2-scope-定义方式",
            "children": []
          },
          {
            "level": 3,
            "title": "1.3 自定义 Scope",
            "slug": "_1-3-自定义-scope",
            "link": "#_1-3-自定义-scope",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "2. Bean 的完整生命周期",
        "slug": "_2-bean-的完整生命周期",
        "link": "#_2-bean-的完整生命周期",
        "children": [
          {
            "level": 3,
            "title": "2.1 生命周期流程图",
            "slug": "_2-1-生命周期流程图",
            "link": "#_2-1-生命周期流程图",
            "children": []
          },
          {
            "level": 3,
            "title": "2.2 生命周期详解",
            "slug": "_2-2-生命周期详解",
            "link": "#_2-2-生命周期详解",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "3. 生命周期回调方法",
        "slug": "_3-生命周期回调方法",
        "link": "#_3-生命周期回调方法",
        "children": [
          {
            "level": 3,
            "title": "3.1 JSR-250 注解（推荐方式）",
            "slug": "_3-1-jsr-250-注解-推荐方式",
            "link": "#_3-1-jsr-250-注解-推荐方式",
            "children": []
          },
          {
            "level": 3,
            "title": "3.2 实现 InitializingBean 和 DisposableBean",
            "slug": "_3-2-实现-initializingbean-和-disposablebean",
            "link": "#_3-2-实现-initializingbean-和-disposablebean",
            "children": []
          },
          {
            "level": 3,
            "title": "3.3 自定义 init-method 和 destroy-method",
            "slug": "_3-3-自定义-init-method-和-destroy-method",
            "link": "#_3-3-自定义-init-method-和-destroy-method",
            "children": []
          },
          {
            "level": 3,
            "title": "3.4 多种初始化方式的执行顺序",
            "slug": "_3-4-多种初始化方式的执行顺序",
            "link": "#_3-4-多种初始化方式的执行顺序",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "4. Bean 的作用域与生命周期关系",
        "slug": "_4-bean-的作用域与生命周期关系",
        "link": "#_4-bean-的作用域与生命周期关系",
        "children": [
          {
            "level": 3,
            "title": "4.1 Singleton Bean 的生命周期",
            "slug": "_4-1-singleton-bean-的生命周期",
            "link": "#_4-1-singleton-bean-的生命周期",
            "children": []
          },
          {
            "level": 3,
            "title": "4.2 Prototype Bean 的生命周期",
            "slug": "_4-2-prototype-bean-的生命周期",
            "link": "#_4-2-prototype-bean-的生命周期",
            "children": []
          },
          {
            "level": 3,
            "title": "4.3 Request Bean 的生命周期（Web 环境）",
            "slug": "_4-3-request-bean-的生命周期-web-环境",
            "link": "#_4-3-request-bean-的生命周期-web-环境",
            "children": []
          },
          {
            "level": 3,
            "title": "4.4 Session Bean 的生命周期（Web 环境）",
            "slug": "_4-4-session-bean-的生命周期-web-环境",
            "link": "#_4-4-session-bean-的生命周期-web-环境",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "5. Aware 接口",
        "slug": "_5-aware-接口",
        "link": "#_5-aware-接口",
        "children": [
          {
            "level": 3,
            "title": "5.1 常用 Aware 接口列表",
            "slug": "_5-1-常用-aware-接口列表",
            "link": "#_5-1-常用-aware-接口列表",
            "children": []
          },
          {
            "level": 3,
            "title": "5.2 Aware 接口使用示例",
            "slug": "_5-2-aware-接口使用示例",
            "link": "#_5-2-aware-接口使用示例",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "6. Bean 后置处理器",
        "slug": "_6-bean-后置处理器",
        "link": "#_6-bean-后置处理器",
        "children": [
          {
            "level": 3,
            "title": "6.1 BeanPostProcessor 接口",
            "slug": "_6-1-beanpostprocessor-接口",
            "link": "#_6-1-beanpostprocessor-接口",
            "children": []
          },
          {
            "level": 3,
            "title": "6.2 常用的内置 BeanPostProcessor",
            "slug": "_6-2-常用的内置-beanpostprocessor",
            "link": "#_6-2-常用的内置-beanpostprocessor",
            "children": []
          },
          {
            "level": 3,
            "title": "6.3 自定义 BeanPostProcessor 实现特定功能",
            "slug": "_6-3-自定义-beanpostprocessor-实现特定功能",
            "link": "#_6-3-自定义-beanpostprocessor-实现特定功能",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "7. 最佳实践",
        "slug": "_7-最佳实践",
        "link": "#_7-最佳实践",
        "children": [
          {
            "level": 3,
            "title": "7.1 Scope 选择指南",
            "slug": "_7-1-scope-选择指南",
            "link": "#_7-1-scope-选择指南",
            "children": []
          },
          {
            "level": 3,
            "title": "7.2 生命周期管理最佳实践",
            "slug": "_7-2-生命周期管理最佳实践",
            "link": "#_7-2-生命周期管理最佳实践",
            "children": []
          },
          {
            "level": 3,
            "title": "7.3 处理 Scope 代理",
            "slug": "_7-3-处理-scope-代理",
            "link": "#_7-3-处理-scope-代理",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "8. 常见问题与解决方案",
        "slug": "_8-常见问题与解决方案",
        "link": "#_8-常见问题与解决方案",
        "children": [
          {
            "level": 3,
            "title": "8.1 Prototype Bean 的销毁问题",
            "slug": "_8-1-prototype-bean-的销毁问题",
            "link": "#_8-1-prototype-bean-的销毁问题",
            "children": []
          },
          {
            "level": 3,
            "title": "8.2 循环依赖问题",
            "slug": "_8-2-循环依赖问题",
            "link": "#_8-2-循环依赖问题",
            "children": []
          },
          {
            "level": 3,
            "title": "8.3 不同 Scope Bean 之间的交互",
            "slug": "_8-3-不同-scope-bean-之间的交互",
            "link": "#_8-3-不同-scope-bean-之间的交互",
            "children": []
          },
          {
            "level": 3,
            "title": "8.4 性能优化建议",
            "slug": "_8-4-性能优化建议",
            "link": "#_8-4-性能优化建议",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "总结",
        "slug": "总结",
        "link": "#总结",
        "children": []
      }
    ],
    "path": "/08-Spring%20Bean%E7%94%9F%E5%91%BD%E5%91%A8%E6%9C%9F%E4%B8%8EScope%E7%9F%A5%E8%AF%86%E7%82%B9%E6%80%BB%E7%BB%93.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "Apollo MMORPG 服务器综合架构设计方案",
    "headers": [
      {
        "level": 2,
        "title": "1. 架构设计哲学",
        "slug": "_1-架构设计哲学",
        "link": "#_1-架构设计哲学",
        "children": [
          {
            "level": 3,
            "title": "1.1 核心原则",
            "slug": "_1-1-核心原则",
            "link": "#_1-1-核心原则",
            "children": []
          },
          {
            "level": 3,
            "title": "1.2 架构分层",
            "slug": "_1-2-架构分层",
            "link": "#_1-2-架构分层",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "2. 核心服务详细设计",
        "slug": "_2-核心服务详细设计",
        "link": "#_2-核心服务详细设计",
        "children": [
          {
            "level": 3,
            "title": "2.1 网关服务 (GateServer)",
            "slug": "_2-1-网关服务-gateserver",
            "link": "#_2-1-网关服务-gateserver",
            "children": []
          },
          {
            "level": 3,
            "title": "2.2 场景服务 (ZoneServer)",
            "slug": "_2-2-场景服务-zoneserver",
            "link": "#_2-2-场景服务-zoneserver",
            "children": []
          },
          {
            "level": 3,
            "title": "2.3 AOI服务 (AOIService)",
            "slug": "_2-3-aoi服务-aoiservice",
            "link": "#_2-3-aoi服务-aoiservice",
            "children": []
          },
          {
            "level": 3,
            "title": "2.4 战斗服务 (BattleServer)",
            "slug": "_2-4-战斗服务-battleserver",
            "link": "#_2-4-战斗服务-battleserver",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "3. 数据架构设计",
        "slug": "_3-数据架构设计",
        "link": "#_3-数据架构设计",
        "children": [
          {
            "level": 3,
            "title": "3.1 存储方案",
            "slug": "_3-1-存储方案",
            "link": "#_3-1-存储方案",
            "children": []
          },
          {
            "level": 3,
            "title": "3.2 数据同步机制",
            "slug": "_3-2-数据同步机制",
            "link": "#_3-2-数据同步机制",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "4. 服务治理与运维",
        "slug": "_4-服务治理与运维",
        "link": "#_4-服务治理与运维",
        "children": [
          {
            "level": 3,
            "title": "4.1 服务发现",
            "slug": "_4-1-服务发现",
            "link": "#_4-1-服务发现",
            "children": []
          },
          {
            "level": 3,
            "title": "4.2 监控体系",
            "slug": "_4-2-监控体系",
            "link": "#_4-2-监控体系",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "5. 性能优化策略",
        "slug": "_5-性能优化策略",
        "link": "#_5-性能优化策略",
        "children": [
          {
            "level": 3,
            "title": "5.1 网络优化",
            "slug": "_5-1-网络优化",
            "link": "#_5-1-网络优化",
            "children": []
          },
          {
            "level": 3,
            "title": "5.2 内存管理",
            "slug": "_5-2-内存管理",
            "link": "#_5-2-内存管理",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "6. 安全设计",
        "slug": "_6-安全设计",
        "link": "#_6-安全设计",
        "children": [
          {
            "level": 3,
            "title": "6.1 通信安全",
            "slug": "_6-1-通信安全",
            "link": "#_6-1-通信安全",
            "children": []
          },
          {
            "level": 3,
            "title": "6.2 游戏安全",
            "slug": "_6-2-游戏安全",
            "link": "#_6-2-游戏安全",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "7. 部署架构",
        "slug": "_7-部署架构",
        "link": "#_7-部署架构",
        "children": [
          {
            "level": 3,
            "title": "7.1 容器化部署",
            "slug": "_7-1-容器化部署",
            "link": "#_7-1-容器化部署",
            "children": []
          },
          {
            "level": 3,
            "title": "7.2 自动扩缩容",
            "slug": "_7-2-自动扩缩容",
            "link": "#_7-2-自动扩缩容",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "8. 开发流程",
        "slug": "_8-开发流程",
        "link": "#_8-开发流程",
        "children": [
          {
            "level": 3,
            "title": "8.1 代码规范",
            "slug": "_8-1-代码规范",
            "link": "#_8-1-代码规范",
            "children": []
          },
          {
            "level": 3,
            "title": "8.2 CI/CD流程",
            "slug": "_8-2-ci-cd流程",
            "link": "#_8-2-ci-cd流程",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "9. 实施路线图",
        "slug": "_9-实施路线图",
        "link": "#_9-实施路线图",
        "children": [
          {
            "level": 3,
            "title": "Phase 1: 基础框架 (2个月)",
            "slug": "phase-1-基础框架-2个月",
            "link": "#phase-1-基础框架-2个月",
            "children": []
          },
          {
            "level": 3,
            "title": "Phase 2: 核心服务 (3个月)",
            "slug": "phase-2-核心服务-3个月",
            "link": "#phase-2-核心服务-3个月",
            "children": []
          },
          {
            "level": 3,
            "title": "Phase 3: 业务系统 (4个月)",
            "slug": "phase-3-业务系统-4个月",
            "link": "#phase-3-业务系统-4个月",
            "children": []
          },
          {
            "level": 3,
            "title": "Phase 4: 运维体系 (2个月)",
            "slug": "phase-4-运维体系-2个月",
            "link": "#phase-4-运维体系-2个月",
            "children": []
          },
          {
            "level": 3,
            "title": "Phase 5: 上线运营 (1个月)",
            "slug": "phase-5-上线运营-1个月",
            "link": "#phase-5-上线运营-1个月",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "10. 总结",
        "slug": "_10-总结",
        "link": "#_10-总结",
        "children": []
      }
    ],
    "path": "/09-Apollo_Comprehensive_Architecture.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "Apollo 技术选型与对比分析",
    "headers": [
      {
        "level": 2,
        "title": "1. 编程语言选择",
        "slug": "_1-编程语言选择",
        "link": "#_1-编程语言选择",
        "children": [
          {
            "level": 3,
            "title": "1.1 候选语言对比",
            "slug": "_1-1-候选语言对比",
            "link": "#_1-1-候选语言对比",
            "children": []
          },
          {
            "level": 3,
            "title": "1.2 选择 C++ 的理由",
            "slug": "_1-2-选择-c-的理由",
            "link": "#_1-2-选择-c-的理由",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "2. 网络框架选择",
        "slug": "_2-网络框架选择",
        "link": "#_2-网络框架选择",
        "children": [
          {
            "level": 3,
            "title": "2.1 候选方案对比",
            "slug": "_2-1-候选方案对比",
            "link": "#_2-1-候选方案对比",
            "children": []
          },
          {
            "level": 3,
            "title": "2.2 选择 libevent 的理由",
            "slug": "_2-2-选择-libevent-的理由",
            "link": "#_2-2-选择-libevent-的理由",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "3. 序列化协议选择",
        "slug": "_3-序列化协议选择",
        "link": "#_3-序列化协议选择",
        "children": [
          {
            "level": 3,
            "title": "3.1 对比分析",
            "slug": "_3-1-对比分析",
            "link": "#_3-1-对比分析",
            "children": []
          },
          {
            "level": 3,
            "title": "3.2 Protobuf 优势",
            "slug": "_3-2-protobuf-优势",
            "link": "#_3-2-protobuf-优势",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "4. 数据库选择",
        "slug": "_4-数据库选择",
        "link": "#_4-数据库选择",
        "children": [
          {
            "level": 3,
            "title": "4.1 主数据库方案",
            "slug": "_4-1-主数据库方案",
            "link": "#_4-1-主数据库方案",
            "children": []
          },
          {
            "level": 3,
            "title": "4.2 缓存方案",
            "slug": "_4-2-缓存方案",
            "link": "#_4-2-缓存方案",
            "children": []
          },
          {
            "level": 3,
            "title": "4.3 MySQL + Redis 组合方案",
            "slug": "_4-3-mysql-redis-组合方案",
            "link": "#_4-3-mysql-redis-组合方案",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "5. 消息队列选择",
        "slug": "_5-消息队列选择",
        "link": "#_5-消息队列选择",
        "children": [
          {
            "level": 3,
            "title": "5.1 候选方案",
            "slug": "_5-1-候选方案",
            "link": "#_5-1-候选方案",
            "children": []
          },
          {
            "level": 3,
            "title": "5.2 使用场景划分",
            "slug": "_5-2-使用场景划分",
            "link": "#_5-2-使用场景划分",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "6. 容器化技术选择",
        "slug": "_6-容器化技术选择",
        "link": "#_6-容器化技术选择",
        "children": [
          {
            "level": 3,
            "title": "6.1 容器运行时",
            "slug": "_6-1-容器运行时",
            "link": "#_6-1-容器运行时",
            "children": []
          },
          {
            "level": 3,
            "title": "6.2 编排平台",
            "slug": "_6-2-编排平台",
            "link": "#_6-2-编排平台",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "7. 监控与日志",
        "slug": "_7-监控与日志",
        "link": "#_7-监控与日志",
        "children": [
          {
            "level": 3,
            "title": "7.1 监控方案",
            "slug": "_7-1-监控方案",
            "link": "#_7-1-监控方案",
            "children": []
          },
          {
            "level": 3,
            "title": "7.2 日志方案",
            "slug": "_7-2-日志方案",
            "link": "#_7-2-日志方案",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "8. 技术选型最终决策",
        "slug": "_8-技术选型最终决策",
        "link": "#_8-技术选型最终决策",
        "children": [
          {
            "level": 3,
            "title": "8.1 核心技术栈",
            "slug": "_8-1-核心技术栈",
            "link": "#_8-1-核心技术栈",
            "children": []
          },
          {
            "level": 3,
            "title": "8.2 选型原则总结",
            "slug": "_8-2-选型原则总结",
            "link": "#_8-2-选型原则总结",
            "children": []
          },
          {
            "level": 3,
            "title": "8.3 技术债务管理",
            "slug": "_8-3-技术债务管理",
            "link": "#_8-3-技术债务管理",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "9. 实施建议",
        "slug": "_9-实施建议",
        "link": "#_9-实施建议",
        "children": [
          {
            "level": 3,
            "title": "9.1 技术预研阶段 (2周)",
            "slug": "_9-1-技术预研阶段-2周",
            "link": "#_9-1-技术预研阶段-2周",
            "children": []
          },
          {
            "level": 3,
            "title": "9.2 技术培训 (1周)",
            "slug": "_9-2-技术培训-1周",
            "link": "#_9-2-技术培训-1周",
            "children": []
          },
          {
            "level": 3,
            "title": "9.3 渐进式迁移",
            "slug": "_9-3-渐进式迁移",
            "link": "#_9-3-渐进式迁移",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "10. 风险评估",
        "slug": "_10-风险评估",
        "link": "#_10-风险评估",
        "children": [
          {
            "level": 3,
            "title": "10.1 技术风险",
            "slug": "_10-1-技术风险",
            "link": "#_10-1-技术风险",
            "children": []
          },
          {
            "level": 3,
            "title": "10.2 业务风险",
            "slug": "_10-2-业务风险",
            "link": "#_10-2-业务风险",
            "children": []
          }
        ]
      }
    ],
    "path": "/10-Technology_Selection.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "Apollo MMORPG 服务器实施计划",
    "headers": [
      {
        "level": 2,
        "title": "1. 项目目标",
        "slug": "_1-项目目标",
        "link": "#_1-项目目标",
        "children": [
          {
            "level": 3,
            "title": "1.1 核心指标",
            "slug": "_1-1-核心指标",
            "link": "#_1-1-核心指标",
            "children": []
          },
          {
            "level": 3,
            "title": "1.2 验收标准",
            "slug": "_1-2-验收标准",
            "link": "#_1-2-验收标准",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "2. 团队组织",
        "slug": "_2-团队组织",
        "link": "#_2-团队组织",
        "children": []
      },
      {
        "level": 2,
        "title": "3. 阶段规划",
        "slug": "_3-阶段规划",
        "link": "#_3-阶段规划",
        "children": [
          {
            "level": 3,
            "title": "Phase 1：基础架构搭建 (2025.01 - 2025.03)",
            "slug": "phase-1-基础架构搭建-2025-01-2025-03",
            "link": "#phase-1-基础架构搭建-2025-01-2025-03",
            "children": []
          },
          {
            "level": 3,
            "title": "Phase 2：核心服务开发 (2025.04 - 2025.07)",
            "slug": "phase-2-核心服务开发-2025-04-2025-07",
            "link": "#phase-2-核心服务开发-2025-04-2025-07",
            "children": []
          },
          {
            "level": 3,
            "title": "Phase 3：系统强化与玩法 (2025.08 - 2025.10)",
            "slug": "phase-3-系统强化与玩法-2025-08-2025-10",
            "link": "#phase-3-系统强化与玩法-2025-08-2025-10",
            "children": []
          },
          {
            "level": 3,
            "title": "Phase 4：稳定化与上线准备 (2025.11 - 2025.12)",
            "slug": "phase-4-稳定化与上线准备-2025-11-2025-12",
            "link": "#phase-4-稳定化与上线准备-2025-11-2025-12",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "4. 关键依赖与风险",
        "slug": "_4-关键依赖与风险",
        "link": "#_4-关键依赖与风险",
        "children": []
      },
      {
        "level": 2,
        "title": "5. 追踪方式",
        "slug": "_5-追踪方式",
        "link": "#_5-追踪方式",
        "children": []
      }
    ],
    "path": "/11-Implementation_Plan.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "Apollo MMORPG API 设计文档",
    "headers": [
      {
        "level": 2,
        "title": "1. API 设计原则",
        "slug": "_1-api-设计原则",
        "link": "#_1-api-设计原则",
        "children": [
          {
            "level": 3,
            "title": "1.1 设计理念",
            "slug": "_1-1-设计理念",
            "link": "#_1-1-设计理念",
            "children": []
          },
          {
            "level": 3,
            "title": "1.2 命名规范",
            "slug": "_1-2-命名规范",
            "link": "#_1-2-命名规范",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "2. 通用数据结构",
        "slug": "_2-通用数据结构",
        "link": "#_2-通用数据结构",
        "children": [
          {
            "level": 3,
            "title": "2.1 基础类型定义",
            "slug": "_2-1-基础类型定义",
            "link": "#_2-1-基础类型定义",
            "children": []
          },
          {
            "level": 3,
            "title": "2.2 通用请求响应",
            "slug": "_2-2-通用请求响应",
            "link": "#_2-2-通用请求响应",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "3. 玩家服务 API",
        "slug": "_3-玩家服务-api",
        "link": "#_3-玩家服务-api",
        "children": [
          {
            "level": 3,
            "title": "3.1 玩家基础信息",
            "slug": "_3-1-玩家基础信息",
            "link": "#_3-1-玩家基础信息",
            "children": []
          },
          {
            "level": 3,
            "title": "3.2 玩家属性管理",
            "slug": "_3-2-玩家属性管理",
            "link": "#_3-2-玩家属性管理",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "4. 战斗系统 API",
        "slug": "_4-战斗系统-api",
        "link": "#_4-战斗系统-api",
        "children": [
          {
            "level": 3,
            "title": "4.1 技能系统",
            "slug": "_4-1-技能系统",
            "link": "#_4-1-技能系统",
            "children": []
          },
          {
            "level": 3,
            "title": "4.2 战斗匹配",
            "slug": "_4-2-战斗匹配",
            "link": "#_4-2-战斗匹配",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "5. 社交系统 API",
        "slug": "_5-社交系统-api",
        "link": "#_5-社交系统-api",
        "children": [
          {
            "level": 3,
            "title": "5.1 好友系统",
            "slug": "_5-1-好友系统",
            "link": "#_5-1-好友系统",
            "children": []
          },
          {
            "level": 3,
            "title": "5.2 公会系统",
            "slug": "_5-2-公会系统",
            "link": "#_5-2-公会系统",
            "children": []
          },
          {
            "level": 3,
            "title": "5.3 聊天系统",
            "slug": "_5-3-聊天系统",
            "link": "#_5-3-聊天系统",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "6. 场景与移动 API",
        "slug": "_6-场景与移动-api",
        "link": "#_6-场景与移动-api",
        "children": [
          {
            "level": 3,
            "title": "6.1 场景管理",
            "slug": "_6-1-场景管理",
            "link": "#_6-1-场景管理",
            "children": []
          },
          {
            "level": 3,
            "title": "6.2 AOI 服务",
            "slug": "_6-2-aoi-服务",
            "link": "#_6-2-aoi-服务",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "7. 任务系统 API",
        "slug": "_7-任务系统-api",
        "link": "#_7-任务系统-api",
        "children": [
          {
            "level": 3,
            "title": "7.1 任务管理",
            "slug": "_7-1-任务管理",
            "link": "#_7-1-任务管理",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "8. 经济系统 API",
        "slug": "_8-经济系统-api",
        "link": "#_8-经济系统-api",
        "children": [
          {
            "level": 3,
            "title": "8.1 商城系统",
            "slug": "_8-1-商城系统",
            "link": "#_8-1-商城系统",
            "children": []
          },
          {
            "level": 3,
            "title": "8.2 交易系统",
            "slug": "_8-2-交易系统",
            "link": "#_8-2-交易系统",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "9. 排行榜 API",
        "slug": "_9-排行榜-api",
        "link": "#_9-排行榜-api",
        "children": [
          {
            "level": 3,
            "title": "9.1 排行榜管理",
            "slug": "_9-1-排行榜管理",
            "link": "#_9-1-排行榜管理",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "10. 系统管理 API",
        "slug": "_10-系统管理-api",
        "link": "#_10-系统管理-api",
        "children": [
          {
            "level": 3,
            "title": "10.1 服务器管理",
            "slug": "_10-1-服务器管理",
            "link": "#_10-1-服务器管理",
            "children": []
          },
          {
            "level": 3,
            "title": "10.2 监控 API",
            "slug": "_10-2-监控-api",
            "link": "#_10-2-监控-api",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "11. 错误码定义",
        "slug": "_11-错误码定义",
        "link": "#_11-错误码定义",
        "children": [
          {
            "level": 3,
            "title": "11.1 全局错误码",
            "slug": "_11-1-全局错误码",
            "link": "#_11-1-全局错误码",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "12. API 版本管理",
        "slug": "_12-api-版本管理",
        "link": "#_12-api-版本管理",
        "children": [
          {
            "level": 3,
            "title": "12.1 版本策略",
            "slug": "_12-1-版本策略",
            "link": "#_12-1-版本策略",
            "children": []
          },
          {
            "level": 3,
            "title": "12.2 路由策略",
            "slug": "_12-2-路由策略",
            "link": "#_12-2-路由策略",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "13. 安全规范",
        "slug": "_13-安全规范",
        "link": "#_13-安全规范",
        "children": [
          {
            "level": 3,
            "title": "13.1 认证授权",
            "slug": "_13-1-认证授权",
            "link": "#_13-1-认证授权",
            "children": []
          },
          {
            "level": 3,
            "title": "13.2 签名验证",
            "slug": "_13-2-签名验证",
            "link": "#_13-2-签名验证",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "14. 使用示例",
        "slug": "_14-使用示例",
        "link": "#_14-使用示例",
        "children": [
          {
            "level": 3,
            "title": "14.1 客户端调用示例",
            "slug": "_14-1-客户端调用示例",
            "link": "#_14-1-客户端调用示例",
            "children": []
          },
          {
            "level": 3,
            "title": "14.2 服务器间调用示例",
            "slug": "_14-2-服务器间调用示例",
            "link": "#_14-2-服务器间调用示例",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "15. 总结",
        "slug": "_15-总结",
        "link": "#_15-总结",
        "children": []
      }
    ],
    "path": "/12-API_Design.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "Apollo 进程间通信 Transport 设计",
    "headers": [
      {
        "level": 2,
        "title": "1. 设计原则",
        "slug": "_1-设计原则",
        "link": "#_1-设计原则",
        "children": []
      },
      {
        "level": 2,
        "title": "2. Transport 接口",
        "slug": "_2-transport-接口",
        "link": "#_2-transport-接口",
        "children": [
          {
            "level": 3,
            "title": "默认实现（v1）",
            "slug": "默认实现-v1",
            "link": "#默认实现-v1",
            "children": []
          },
          {
            "level": 3,
            "title": "背压/统计",
            "slug": "背压-统计",
            "link": "#背压-统计",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "3. 服务注册与能力描述",
        "slug": "_3-服务注册与能力描述",
        "link": "#_3-服务注册与能力描述",
        "children": [
          {
            "level": 3,
            "title": "注册信息结构",
            "slug": "注册信息结构",
            "link": "#注册信息结构",
            "children": []
          },
          {
            "level": 3,
            "title": "注册流程",
            "slug": "注册流程",
            "link": "#注册流程",
            "children": []
          },
          {
            "level": 3,
            "title": "消费流程",
            "slug": "消费流程",
            "link": "#消费流程",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "4. 能力协商与特性",
        "slug": "_4-能力协商与特性",
        "link": "#_4-能力协商与特性",
        "children": []
      },
      {
        "level": 2,
        "title": "5. 监控与运维",
        "slug": "_5-监控与运维",
        "link": "#_5-监控与运维",
        "children": []
      },
      {
        "level": 2,
        "title": "6. Roadmap",
        "slug": "_6-roadmap",
        "link": "#_6-roadmap",
        "children": []
      }
    ],
    "path": "/13-Transport_Design.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "Spring Boot 功能对照与 Apollo 实现建议",
    "headers": [
      {
        "level": 2,
        "title": "1. 功能映射概览",
        "slug": "_1-功能映射概览",
        "link": "#_1-功能映射概览",
        "children": []
      },
      {
        "level": 2,
        "title": "2. 建议优先实现的通用组件",
        "slug": "_2-建议优先实现的通用组件",
        "link": "#_2-建议优先实现的通用组件",
        "children": [
          {
            "level": 3,
            "title": "2.1 ApplicationContext 增强",
            "slug": "_2-1-applicationcontext-增强",
            "link": "#_2-1-applicationcontext-增强",
            "children": []
          },
          {
            "level": 3,
            "title": "2.2 配置系统升级",
            "slug": "_2-2-配置系统升级",
            "link": "#_2-2-配置系统升级",
            "children": []
          },
          {
            "level": 3,
            "title": "2.3 命令行解析",
            "slug": "_2-3-命令行解析",
            "link": "#_2-3-命令行解析",
            "children": []
          },
          {
            "level": 3,
            "title": "2.4 异步事件/任务",
            "slug": "_2-4-异步事件-任务",
            "link": "#_2-4-异步事件-任务",
            "children": []
          },
          {
            "level": 3,
            "title": "2.5 管理端点/命令",
            "slug": "_2-5-管理端点-命令",
            "link": "#_2-5-管理端点-命令",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "3. 可后续评估的高级能力",
        "slug": "_3-可后续评估的高级能力",
        "link": "#_3-可后续评估的高级能力",
        "children": []
      },
      {
        "level": 2,
        "title": "4. 推荐实施顺序",
        "slug": "_4-推荐实施顺序",
        "link": "#_4-推荐实施顺序",
        "children": []
      }
    ],
    "path": "/14-SpringLike_Infrastructure.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "插件化与动态加载设计（存储/扩展模块）",
    "headers": [
      {
        "level": 2,
        "title": "1. 设计目标",
        "slug": "_1-设计目标",
        "link": "#_1-设计目标",
        "children": []
      },
      {
        "level": 2,
        "title": "2. SharedLibrary 封装",
        "slug": "_2-sharedlibrary-封装",
        "link": "#_2-sharedlibrary-封装",
        "children": []
      },
      {
        "level": 2,
        "title": "3. 插件接口约定",
        "slug": "_3-插件接口约定",
        "link": "#_3-插件接口约定",
        "children": []
      },
      {
        "level": 2,
        "title": "4. 启动流程",
        "slug": "_4-启动流程",
        "link": "#_4-启动流程",
        "children": []
      },
      {
        "level": 2,
        "title": "5. 数据源插件应用",
        "slug": "_5-数据源插件应用",
        "link": "#_5-数据源插件应用",
        "children": []
      },
      {
        "level": 2,
        "title": "6. 安全/版本控制",
        "slug": "_6-安全-版本控制",
        "link": "#_6-安全-版本控制",
        "children": []
      },
      {
        "level": 2,
        "title": "7. Roadmap",
        "slug": "_7-roadmap",
        "link": "#_7-roadmap",
        "children": []
      }
    ],
    "path": "/15-Plugin_Architecture.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "Apollo NetCore 网络模块设计",
    "headers": [
      {
        "level": 2,
        "title": "1. 设计目标",
        "slug": "_1-设计目标",
        "link": "#_1-设计目标",
        "children": []
      },
      {
        "level": 2,
        "title": "2. 模块分层",
        "slug": "_2-模块分层",
        "link": "#_2-模块分层",
        "children": []
      },
      {
        "level": 2,
        "title": "3. 核心接口",
        "slug": "_3-核心接口",
        "link": "#_3-核心接口",
        "children": [
          {
            "level": 3,
            "title": "SessionManager",
            "slug": "sessionmanager",
            "link": "#sessionmanager",
            "children": []
          },
          {
            "level": 3,
            "title": "MessageCodec / PacketBuilder",
            "slug": "messagecodec-packetbuilder",
            "link": "#messagecodec-packetbuilder",
            "children": []
          },
          {
            "level": 3,
            "title": "RateLimiter / Deduplicator",
            "slug": "ratelimiter-deduplicator",
            "link": "#ratelimiter-deduplicator",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "4. Linux Backend 设计",
        "slug": "_4-linux-backend-设计",
        "link": "#_4-linux-backend-设计",
        "children": [
          {
            "level": 3,
            "title": "4.1 io_uring",
            "slug": "_4-1-io-uring",
            "link": "#_4-1-io-uring",
            "children": []
          },
          {
            "level": 3,
            "title": "4.2 线程模型",
            "slug": "_4-2-线程模型",
            "link": "#_4-2-线程模型",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "5. Windows Backend 设计",
        "slug": "_5-windows-backend-设计",
        "link": "#_5-windows-backend-设计",
        "children": [
          {
            "level": 3,
            "title": "5.1 IOCP",
            "slug": "_5-1-iocp",
            "link": "#_5-1-iocp",
            "children": []
          },
          {
            "level": 3,
            "title": "5.2 KCP/WebSocket (可选)",
            "slug": "_5-2-kcp-websocket-可选",
            "link": "#_5-2-kcp-websocket-可选",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "6. 客户端 SDK 接入规范",
        "slug": "_6-客户端-sdk-接入规范",
        "link": "#_6-客户端-sdk-接入规范",
        "children": [
          {
            "level": 3,
            "title": "6.1 SDK 特性",
            "slug": "_6-1-sdk-特性",
            "link": "#_6-1-sdk-特性",
            "children": []
          },
          {
            "level": 3,
            "title": "6.2 握手流程",
            "slug": "_6-2-握手流程",
            "link": "#_6-2-握手流程",
            "children": []
          },
          {
            "level": 3,
            "title": "6.3 协议规范",
            "slug": "_6-3-协议规范",
            "link": "#_6-3-协议规范",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "7. 安全与加密",
        "slug": "_7-安全与加密",
        "link": "#_7-安全与加密",
        "children": []
      },
      {
        "level": 2,
        "title": "8. 灰度与流量复制",
        "slug": "_8-灰度与流量复制",
        "link": "#_8-灰度与流量复制",
        "children": []
      },
      {
        "level": 2,
        "title": "9. 监控与调试",
        "slug": "_9-监控与调试",
        "link": "#_9-监控与调试",
        "children": []
      },
      {
        "level": 2,
        "title": "10. 项目拆分",
        "slug": "_10-项目拆分",
        "link": "#_10-项目拆分",
        "children": []
      }
    ],
    "path": "/16-NetCore_Design.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "AOI 服务与场景调度设计",
    "headers": [
      {
        "level": 2,
        "title": "1. 设计目标",
        "slug": "_1-设计目标",
        "link": "#_1-设计目标",
        "children": []
      },
      {
        "level": 2,
        "title": "2. 组件概览",
        "slug": "_2-组件概览",
        "link": "#_2-组件概览",
        "children": []
      },
      {
        "level": 2,
        "title": "3. Scene Orchestrator",
        "slug": "_3-scene-orchestrator",
        "link": "#_3-scene-orchestrator",
        "children": [
          {
            "level": 3,
            "title": "3.1 职责",
            "slug": "_3-1-职责",
            "link": "#_3-1-职责",
            "children": []
          },
          {
            "level": 3,
            "title": "3.2 实现要点",
            "slug": "_3-2-实现要点",
            "link": "#_3-2-实现要点",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "4. ZoneServer",
        "slug": "_4-zoneserver",
        "link": "#_4-zoneserver",
        "children": [
          {
            "level": 3,
            "title": "4.1 职责",
            "slug": "_4-1-职责",
            "link": "#_4-1-职责",
            "children": []
          },
          {
            "level": 3,
            "title": "4.2 ECS & 事件流",
            "slug": "_4-2-ecs-事件流",
            "link": "#_4-2-ecs-事件流",
            "children": []
          },
          {
            "level": 3,
            "title": "4.3 线程模型",
            "slug": "_4-3-线程模型",
            "link": "#_4-3-线程模型",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "5. AOI Service",
        "slug": "_5-aoi-service",
        "link": "#_5-aoi-service",
        "children": [
          {
            "level": 3,
            "title": "5.1 数据结构",
            "slug": "_5-1-数据结构",
            "link": "#_5-1-数据结构",
            "children": []
          },
          {
            "level": 3,
            "title": "5.2 功能",
            "slug": "_5-2-功能",
            "link": "#_5-2-功能",
            "children": []
          },
          {
            "level": 3,
            "title": "5.3 通信接口（示例）",
            "slug": "_5-3-通信接口-示例",
            "link": "#_5-3-通信接口-示例",
            "children": []
          },
          {
            "level": 3,
            "title": "5.4 容灾",
            "slug": "_5-4-容灾",
            "link": "#_5-4-容灾",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "6. Battle Service（可选）",
        "slug": "_6-battle-service-可选",
        "link": "#_6-battle-service-可选",
        "children": []
      },
      {
        "level": 2,
        "title": "7. 与 Gate/Guild/Transport 的关系",
        "slug": "_7-与-gate-guild-transport-的关系",
        "link": "#_7-与-gate-guild-transport-的关系",
        "children": []
      },
      {
        "level": 2,
        "title": "8. 监控与运维",
        "slug": "_8-监控与运维",
        "link": "#_8-监控与运维",
        "children": []
      },
      {
        "level": 2,
        "title": "9. 实施 Roadmap",
        "slug": "_9-实施-roadmap",
        "link": "#_9-实施-roadmap",
        "children": []
      }
    ],
    "path": "/17-AOI_and_Scene_Design.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "数据架构白皮书",
    "headers": [
      {
        "level": 2,
        "title": "1. 设计原则",
        "slug": "_1-设计原则",
        "link": "#_1-设计原则",
        "children": []
      },
      {
        "level": 2,
        "title": "2. 数据分类与落地",
        "slug": "_2-数据分类与落地",
        "link": "#_2-数据分类与落地",
        "children": []
      },
      {
        "level": 2,
        "title": "3. Hybrid Schema 详解",
        "slug": "_3-hybrid-schema-详解",
        "link": "#_3-hybrid-schema-详解",
        "children": [
          {
            "level": 3,
            "title": "3.1 表结构（示例 t_character）",
            "slug": "_3-1-表结构-示例-t-character",
            "link": "#_3-1-表结构-示例-t-character",
            "children": []
          },
          {
            "level": 3,
            "title": "3.2 Blob 内容",
            "slug": "_3-2-blob-内容",
            "link": "#_3-2-blob-内容",
            "children": []
          },
          {
            "level": 3,
            "title": "3.3 列化字段",
            "slug": "_3-3-列化字段",
            "link": "#_3-3-列化字段",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "4. DataProxy 层",
        "slug": "_4-dataproxy-层",
        "link": "#_4-dataproxy-层",
        "children": [
          {
            "level": 3,
            "title": "4.1 职责",
            "slug": "_4-1-职责",
            "link": "#_4-1-职责",
            "children": []
          },
          {
            "level": 3,
            "title": "4.2 写策略",
            "slug": "_4-2-写策略",
            "link": "#_4-2-写策略",
            "children": []
          },
          {
            "level": 3,
            "title": "4.3 缓存策略",
            "slug": "_4-3-缓存策略",
            "link": "#_4-3-缓存策略",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "5. 分库分表与扩容",
        "slug": "_5-分库分表与扩容",
        "link": "#_5-分库分表与扩容",
        "children": [
          {
            "level": 3,
            "title": "5.1 分片规则",
            "slug": "_5-1-分片规则",
            "link": "#_5-1-分片规则",
            "children": []
          },
          {
            "level": 3,
            "title": "5.2 索引与归档",
            "slug": "_5-2-索引与归档",
            "link": "#_5-2-索引与归档",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "6. TLog / BI 流水",
        "slug": "_6-tlog-bi-流水",
        "link": "#_6-tlog-bi-流水",
        "children": [
          {
            "level": 3,
            "title": "6.1 流程",
            "slug": "_6-1-流程",
            "link": "#_6-1-流程",
            "children": []
          },
          {
            "level": 3,
            "title": "6.2 日志规范",
            "slug": "_6-2-日志规范",
            "link": "#_6-2-日志规范",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "7. 数据恢复与一致性",
        "slug": "_7-数据恢复与一致性",
        "link": "#_7-数据恢复与一致性",
        "children": []
      },
      {
        "level": 2,
        "title": "8. 工具与流程",
        "slug": "_8-工具与流程",
        "link": "#_8-工具与流程",
        "children": []
      },
      {
        "level": 2,
        "title": "9. 统一决策",
        "slug": "_9-统一决策",
        "link": "#_9-统一决策",
        "children": []
      },
      {
        "level": 2,
        "title": "10. Roadmap",
        "slug": "_10-roadmap",
        "link": "#_10-roadmap",
        "children": []
      }
    ],
    "path": "/18-Data_Architecture_Whitepaper.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "API 与 SDK 规范",
    "headers": [
      {
        "level": 2,
        "title": "1. 总体结构",
        "slug": "_1-总体结构",
        "link": "#_1-总体结构",
        "children": []
      },
      {
        "level": 2,
        "title": "2. 内部 gRPC / TCP API",
        "slug": "_2-内部-grpc-tcp-api",
        "link": "#_2-内部-grpc-tcp-api",
        "children": [
          {
            "level": 3,
            "title": "2.1 命名与版本",
            "slug": "_2-1-命名与版本",
            "link": "#_2-1-命名与版本",
            "children": []
          },
          {
            "level": 3,
            "title": "2.2 认证",
            "slug": "_2-2-认证",
            "link": "#_2-2-认证",
            "children": []
          },
          {
            "level": 3,
            "title": "2.3 幂等和重试",
            "slug": "_2-3-幂等和重试",
            "link": "#_2-3-幂等和重试",
            "children": []
          },
          {
            "level": 3,
            "title": "2.4 示例",
            "slug": "_2-4-示例",
            "link": "#_2-4-示例",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "3. 客户端 SDK 接口",
        "slug": "_3-客户端-sdk-接口",
        "link": "#_3-客户端-sdk-接口",
        "children": [
          {
            "level": 3,
            "title": "3.1 SDK 结构",
            "slug": "_3-1-sdk-结构",
            "link": "#_3-1-sdk-结构",
            "children": []
          },
          {
            "level": 3,
            "title": "3.2 关键能力",
            "slug": "_3-2-关键能力",
            "link": "#_3-2-关键能力",
            "children": []
          },
          {
            "level": 3,
            "title": "3.3 API 示例（C#）",
            "slug": "_3-3-api-示例-c",
            "link": "#_3-3-api-示例-c",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "4. 运维/运营 API (HTTP/gRPC)",
        "slug": "_4-运维-运营-api-http-grpc",
        "link": "#_4-运维-运营-api-http-grpc",
        "children": [
          {
            "level": 3,
            "title": "4.1 设计原则",
            "slug": "_4-1-设计原则",
            "link": "#_4-1-设计原则",
            "children": []
          },
          {
            "level": 3,
            "title": "4.2 基础结构",
            "slug": "_4-2-基础结构",
            "link": "#_4-2-基础结构",
            "children": []
          },
          {
            "level": 3,
            "title": "4.3 请求/响应格式",
            "slug": "_4-3-请求-响应格式",
            "link": "#_4-3-请求-响应格式",
            "children": []
          },
          {
            "level": 3,
            "title": "4.4 常见 API 分组",
            "slug": "_4-4-常见-api-分组",
            "link": "#_4-4-常见-api-分组",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "5. 命名与版本策略",
        "slug": "_5-命名与版本策略",
        "link": "#_5-命名与版本策略",
        "children": []
      },
      {
        "level": 2,
        "title": "6. 文档与工具",
        "slug": "_6-文档与工具",
        "link": "#_6-文档与工具",
        "children": []
      },
      {
        "level": 2,
        "title": "7. 安全与审计",
        "slug": "_7-安全与审计",
        "link": "#_7-安全与审计",
        "children": []
      },
      {
        "level": 2,
        "title": "8. Roadmap",
        "slug": "_8-roadmap",
        "link": "#_8-roadmap",
        "children": []
      }
    ],
    "path": "/19-API_and_SDK_Standard.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "DevOps 与监控设计",
    "headers": [
      {
        "level": 2,
        "title": "1. CI/CD 流程",
        "slug": "_1-ci-cd-流程",
        "link": "#_1-ci-cd-流程",
        "children": [
          {
            "level": 3,
            "title": "1.1 构建",
            "slug": "_1-1-构建",
            "link": "#_1-1-构建",
            "children": []
          },
          {
            "level": 3,
            "title": "1.2 测试阶段",
            "slug": "_1-2-测试阶段",
            "link": "#_1-2-测试阶段",
            "children": []
          },
          {
            "level": 3,
            "title": "1.3 发布",
            "slug": "_1-3-发布",
            "link": "#_1-3-发布",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "2. 监控与指标",
        "slug": "_2-监控与指标",
        "link": "#_2-监控与指标",
        "children": [
          {
            "level": 3,
            "title": "2.1 指标体系",
            "slug": "_2-1-指标体系",
            "link": "#_2-1-指标体系",
            "children": []
          },
          {
            "level": 3,
            "title": "2.2 采集方式",
            "slug": "_2-2-采集方式",
            "link": "#_2-2-采集方式",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "3. 日志与追踪",
        "slug": "_3-日志与追踪",
        "link": "#_3-日志与追踪",
        "children": [
          {
            "level": 3,
            "title": "3.1 日志",
            "slug": "_3-1-日志",
            "link": "#_3-1-日志",
            "children": []
          },
          {
            "level": 3,
            "title": "3.2 分布式追踪",
            "slug": "_3-2-分布式追踪",
            "link": "#_3-2-分布式追踪",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "4. 告警与自愈",
        "slug": "_4-告警与自愈",
        "link": "#_4-告警与自愈",
        "children": [
          {
            "level": 3,
            "title": "4.1 告警规则",
            "slug": "_4-1-告警规则",
            "link": "#_4-1-告警规则",
            "children": []
          },
          {
            "level": 3,
            "title": "4.2 告警通道",
            "slug": "_4-2-告警通道",
            "link": "#_4-2-告警通道",
            "children": []
          },
          {
            "level": 3,
            "title": "4.3 自愈策略",
            "slug": "_4-3-自愈策略",
            "link": "#_4-3-自愈策略",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "5. 运维流程",
        "slug": "_5-运维流程",
        "link": "#_5-运维流程",
        "children": [
          {
            "level": 3,
            "title": "5.1 配置与发布",
            "slug": "_5-1-配置与发布",
            "link": "#_5-1-配置与发布",
            "children": []
          },
          {
            "level": 3,
            "title": "5.2 故障处置",
            "slug": "_5-2-故障处置",
            "link": "#_5-2-故障处置",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "6. 环境划分",
        "slug": "_6-环境划分",
        "link": "#_6-环境划分",
        "children": []
      },
      {
        "level": 2,
        "title": "7. 工具与自动化",
        "slug": "_7-工具与自动化",
        "link": "#_7-工具与自动化",
        "children": []
      },
      {
        "level": 2,
        "title": "8. 安全与合规",
        "slug": "_8-安全与合规",
        "link": "#_8-安全与合规",
        "children": []
      },
      {
        "level": 2,
        "title": "9. Roadmap",
        "slug": "_9-roadmap",
        "link": "#_9-roadmap",
        "children": []
      }
    ],
    "path": "/20-DevOps_and_Monitoring.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "安全策略设计",
    "headers": [
      {
        "level": 2,
        "title": "1. 威胁模型概览",
        "slug": "_1-威胁模型概览",
        "link": "#_1-威胁模型概览",
        "children": []
      },
      {
        "level": 2,
        "title": "2. 客户端通信安全",
        "slug": "_2-客户端通信安全",
        "link": "#_2-客户端通信安全",
        "children": [
          {
            "level": 3,
            "title": "2.1 连接加密",
            "slug": "_2-1-连接加密",
            "link": "#_2-1-连接加密",
            "children": []
          },
          {
            "level": 3,
            "title": "2.2 消息安全",
            "slug": "_2-2-消息安全",
            "link": "#_2-2-消息安全",
            "children": []
          },
          {
            "level": 3,
            "title": "2.3 客户端验证",
            "slug": "_2-3-客户端验证",
            "link": "#_2-3-客户端验证",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "3. 服务间安全",
        "slug": "_3-服务间安全",
        "link": "#_3-服务间安全",
        "children": [
          {
            "level": 3,
            "title": "3.1 认证授权",
            "slug": "_3-1-认证授权",
            "link": "#_3-1-认证授权",
            "children": []
          },
          {
            "level": 3,
            "title": "3.2 配置与密钥管理",
            "slug": "_3-2-配置与密钥管理",
            "link": "#_3-2-配置与密钥管理",
            "children": []
          },
          {
            "level": 3,
            "title": "3.3 插件安全",
            "slug": "_3-3-插件安全",
            "link": "#_3-3-插件安全",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "4. 运维/GM 安全",
        "slug": "_4-运维-gm-安全",
        "link": "#_4-运维-gm-安全",
        "children": [
          {
            "level": 3,
            "title": "4.1 鉴权",
            "slug": "_4-1-鉴权",
            "link": "#_4-1-鉴权",
            "children": []
          },
          {
            "level": 3,
            "title": "4.2 审计",
            "slug": "_4-2-审计",
            "link": "#_4-2-审计",
            "children": []
          },
          {
            "level": 3,
            "title": "4.3 最小权限",
            "slug": "_4-3-最小权限",
            "link": "#_4-3-最小权限",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "5. 数据与日志安全",
        "slug": "_5-数据与日志安全",
        "link": "#_5-数据与日志安全",
        "children": []
      },
      {
        "level": 2,
        "title": "6. 网络与基建",
        "slug": "_6-网络与基建",
        "link": "#_6-网络与基建",
        "children": []
      },
      {
        "level": 2,
        "title": "7. 故障与应急",
        "slug": "_7-故障与应急",
        "link": "#_7-故障与应急",
        "children": []
      },
      {
        "level": 2,
        "title": "8. Roadmap",
        "slug": "_8-roadmap",
        "link": "#_8-roadmap",
        "children": []
      }
    ],
    "path": "/21-Security_Strategy.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "属性与 IDL 统一方案",
    "headers": [
      {
        "level": 2,
        "title": "1. 目标",
        "slug": "_1-目标",
        "link": "#_1-目标",
        "children": []
      },
      {
        "level": 2,
        "title": "2. 属性定义方案",
        "slug": "_2-属性定义方案",
        "link": "#_2-属性定义方案",
        "children": [
          {
            "level": 3,
            "title": "2.1 数据源",
            "slug": "_2-1-数据源",
            "link": "#_2-1-数据源",
            "children": []
          },
          {
            "level": 3,
            "title": "2.2 生成目标",
            "slug": "_2-2-生成目标",
            "link": "#_2-2-生成目标",
            "children": []
          },
          {
            "level": 3,
            "title": "2.3 工具链",
            "slug": "_2-3-工具链",
            "link": "#_2-3-工具链",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "3. 消息 ID / Proto",
        "slug": "_3-消息-id-proto",
        "link": "#_3-消息-id-proto",
        "children": [
          {
            "level": 3,
            "title": "3.1 结构",
            "slug": "_3-1-结构",
            "link": "#_3-1-结构",
            "children": []
          },
          {
            "level": 3,
            "title": "3.2 生成流程",
            "slug": "_3-2-生成流程",
            "link": "#_3-2-生成流程",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "4. BI/数据同步",
        "slug": "_4-bi-数据同步",
        "link": "#_4-bi-数据同步",
        "children": []
      },
      {
        "level": 2,
        "title": "5. 版本与发布",
        "slug": "_5-版本与发布",
        "link": "#_5-版本与发布",
        "children": []
      },
      {
        "level": 2,
        "title": "6. Roadmap",
        "slug": "_6-roadmap",
        "link": "#_6-roadmap",
        "children": []
      }
    ],
    "path": "/22-Attribute_IDL_Plan.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "性能与容量测试计划",
    "headers": [
      {
        "level": 2,
        "title": "1. 目标",
        "slug": "_1-目标",
        "link": "#_1-目标",
        "children": []
      },
      {
        "level": 2,
        "title": "2. 测试环境与工具",
        "slug": "_2-测试环境与工具",
        "link": "#_2-测试环境与工具",
        "children": [
          {
            "level": 3,
            "title": "2.1 环境",
            "slug": "_2-1-环境",
            "link": "#_2-1-环境",
            "children": []
          },
          {
            "level": 3,
            "title": "2.2 工具",
            "slug": "_2-2-工具",
            "link": "#_2-2-工具",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "3. 模块级性能测试",
        "slug": "_3-模块级性能测试",
        "link": "#_3-模块级性能测试",
        "children": []
      },
      {
        "level": 2,
        "title": "4. 场景/玩法压测",
        "slug": "_4-场景-玩法压测",
        "link": "#_4-场景-玩法压测",
        "children": [
          {
            "level": 3,
            "title": "4.1 基础场景",
            "slug": "_4-1-基础场景",
            "link": "#_4-1-基础场景",
            "children": []
          },
          {
            "level": 3,
            "title": "4.2 战斗场景",
            "slug": "_4-2-战斗场景",
            "link": "#_4-2-战斗场景",
            "children": []
          },
          {
            "level": 3,
            "title": "4.3 社交与经济",
            "slug": "_4-3-社交与经济",
            "link": "#_4-3-社交与经济",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "5. 压测指标与阈值",
        "slug": "_5-压测指标与阈值",
        "link": "#_5-压测指标与阈值",
        "children": []
      },
      {
        "level": 2,
        "title": "6. 故障与可靠性测试",
        "slug": "_6-故障与可靠性测试",
        "link": "#_6-故障与可靠性测试",
        "children": []
      },
      {
        "level": 2,
        "title": "7. 流程与自动化",
        "slug": "_7-流程与自动化",
        "link": "#_7-流程与自动化",
        "children": []
      },
      {
        "level": 2,
        "title": "8. 报告与决策",
        "slug": "_8-报告与决策",
        "link": "#_8-报告与决策",
        "children": []
      },
      {
        "level": 2,
        "title": "9. Roadmap",
        "slug": "_9-roadmap",
        "link": "#_9-roadmap",
        "children": []
      }
    ],
    "path": "/23-Performance_Testing_Plan.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "TLog 与 BI 规范",
    "headers": [
      {
        "level": 2,
        "title": "1. 总体架构",
        "slug": "_1-总体架构",
        "link": "#_1-总体架构",
        "children": []
      },
      {
        "level": 2,
        "title": "2. 日志格式",
        "slug": "_2-日志格式",
        "link": "#_2-日志格式",
        "children": [
          {
            "level": 3,
            "title": "2.1 通用字段",
            "slug": "_2-1-通用字段",
            "link": "#_2-1-通用字段",
            "children": []
          },
          {
            "level": 3,
            "title": "2.2 事件分类",
            "slug": "_2-2-事件分类",
            "link": "#_2-2-事件分类",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "3. 采集与传输",
        "slug": "_3-采集与传输",
        "link": "#_3-采集与传输",
        "children": [
          {
            "level": 3,
            "title": "3.1 LogAgent",
            "slug": "_3-1-logagent",
            "link": "#_3-1-logagent",
            "children": []
          },
          {
            "level": 3,
            "title": "3.2 传输可靠性",
            "slug": "_3-2-传输可靠性",
            "link": "#_3-2-传输可靠性",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "4. Kafka Topic 与 Schema",
        "slug": "_4-kafka-topic-与-schema",
        "link": "#_4-kafka-topic-与-schema",
        "children": []
      },
      {
        "level": 2,
        "title": "5. 实时处理",
        "slug": "_5-实时处理",
        "link": "#_5-实时处理",
        "children": []
      },
      {
        "level": 2,
        "title": "6. 离线处理",
        "slug": "_6-离线处理",
        "link": "#_6-离线处理",
        "children": []
      },
      {
        "level": 2,
        "title": "7. 数据字典与版本",
        "slug": "_7-数据字典与版本",
        "link": "#_7-数据字典与版本",
        "children": []
      },
      {
        "level": 2,
        "title": "8. 安全与隐私",
        "slug": "_8-安全与隐私",
        "link": "#_8-安全与隐私",
        "children": []
      },
      {
        "level": 2,
        "title": "9. 工具与监控",
        "slug": "_9-工具与监控",
        "link": "#_9-工具与监控",
        "children": []
      },
      {
        "level": 2,
        "title": "10. Roadmap",
        "slug": "_10-roadmap",
        "link": "#_10-roadmap",
        "children": []
      }
    ],
    "path": "/24-TLog_and_BI_Spec.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "Battle Service 技术设计",
    "headers": [
      {
        "level": 2,
        "title": "1. 设计目标",
        "slug": "_1-设计目标",
        "link": "#_1-设计目标",
        "children": []
      },
      {
        "level": 2,
        "title": "2. 架构概览",
        "slug": "_2-架构概览",
        "link": "#_2-架构概览",
        "children": []
      },
      {
        "level": 2,
        "title": "3. 功能模块",
        "slug": "_3-功能模块",
        "link": "#_3-功能模块",
        "children": [
          {
            "level": 3,
            "title": "3.1 Matchmaker / Scheduler",
            "slug": "_3-1-matchmaker-scheduler",
            "link": "#_3-1-matchmaker-scheduler",
            "children": []
          },
          {
            "level": 3,
            "title": "3.2 Battle Instance Manager",
            "slug": "_3-2-battle-instance-manager",
            "link": "#_3-2-battle-instance-manager",
            "children": []
          },
          {
            "level": 3,
            "title": "3.3 ECS + Pipeline",
            "slug": "_3-3-ecs-pipeline",
            "link": "#_3-3-ecs-pipeline",
            "children": []
          },
          {
            "level": 3,
            "title": "3.4 Result Dispatcher / Persistence",
            "slug": "_3-4-result-dispatcher-persistence",
            "link": "#_3-4-result-dispatcher-persistence",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "4. 数据模型",
        "slug": "_4-数据模型",
        "link": "#_4-数据模型",
        "children": [
          {
            "level": 3,
            "title": "4.1 通用实体",
            "slug": "_4-1-通用实体",
            "link": "#_4-1-通用实体",
            "children": []
          },
          {
            "level": 3,
            "title": "4.2 事件消息",
            "slug": "_4-2-事件消息",
            "link": "#_4-2-事件消息",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "5. 协议与通信",
        "slug": "_5-协议与通信",
        "link": "#_5-协议与通信",
        "children": []
      },
      {
        "level": 2,
        "title": "6. 可扩展性",
        "slug": "_6-可扩展性",
        "link": "#_6-可扩展性",
        "children": []
      },
      {
        "level": 2,
        "title": "7. 容灾与容错",
        "slug": "_7-容灾与容错",
        "link": "#_7-容灾与容错",
        "children": []
      },
      {
        "level": 2,
        "title": "8. 性能要求",
        "slug": "_8-性能要求",
        "link": "#_8-性能要求",
        "children": []
      },
      {
        "level": 2,
        "title": "9. 监控与运维",
        "slug": "_9-监控与运维",
        "link": "#_9-监控与运维",
        "children": []
      },
      {
        "level": 2,
        "title": "10. 实施计划概述",
        "slug": "_10-实施计划概述",
        "link": "#_10-实施计划概述",
        "children": []
      }
    ],
    "path": "/25-Battle_Service_Design.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "客户端资源与补丁策略",
    "headers": [
      {
        "level": 2,
        "title": "1. 总体流程",
        "slug": "_1-总体流程",
        "link": "#_1-总体流程",
        "children": []
      },
      {
        "level": 2,
        "title": "2. 资源结构",
        "slug": "_2-资源结构",
        "link": "#_2-资源结构",
        "children": []
      },
      {
        "level": 2,
        "title": "3. Unity SDK 资源管理",
        "slug": "_3-unity-sdk-资源管理",
        "link": "#_3-unity-sdk-资源管理",
        "children": [
          {
            "level": 3,
            "title": "3.1 功能",
            "slug": "_3-1-功能",
            "link": "#_3-1-功能",
            "children": []
          },
          {
            "level": 3,
            "title": "3.2 热更新流程",
            "slug": "_3-2-热更新流程",
            "link": "#_3-2-热更新流程",
            "children": []
          },
          {
            "level": 3,
            "title": "3.3 安全",
            "slug": "_3-3-安全",
            "link": "#_3-3-安全",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "4. CDN/发布策略",
        "slug": "_4-cdn-发布策略",
        "link": "#_4-cdn-发布策略",
        "children": [
          {
            "level": 3,
            "title": "4.1 多环境",
            "slug": "_4-1-多环境",
            "link": "#_4-1-多环境",
            "children": []
          },
          {
            "level": 3,
            "title": "4.2 发布流程",
            "slug": "_4-2-发布流程",
            "link": "#_4-2-发布流程",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "5. 服务器接口",
        "slug": "_5-服务器接口",
        "link": "#_5-服务器接口",
        "children": [
          {
            "level": 3,
            "title": "5.1 资源版本 API",
            "slug": "_5-1-资源版本-api",
            "link": "#_5-1-资源版本-api",
            "children": []
          },
          {
            "level": 3,
            "title": "5.2 配置更新",
            "slug": "_5-2-配置更新",
            "link": "#_5-2-配置更新",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "6. 灰度与回滚",
        "slug": "_6-灰度与回滚",
        "link": "#_6-灰度与回滚",
        "children": []
      },
      {
        "level": 2,
        "title": "7. 监控与告警",
        "slug": "_7-监控与告警",
        "link": "#_7-监控与告警",
        "children": []
      },
      {
        "level": 2,
        "title": "8. 实施步骤",
        "slug": "_8-实施步骤",
        "link": "#_8-实施步骤",
        "children": []
      }
    ],
    "path": "/26-Client_Resource_and_Patch.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "核心模块接口与类图",
    "headers": [
      {
        "level": 2,
        "title": "1. NetCore",
        "slug": "_1-netcore",
        "link": "#_1-netcore",
        "children": [
          {
            "level": 3,
            "title": "1.1 类关系",
            "slug": "_1-1-类关系",
            "link": "#_1-1-类关系",
            "children": []
          },
          {
            "level": 3,
            "title": "1.2 关键接口",
            "slug": "_1-2-关键接口",
            "link": "#_1-2-关键接口",
            "children": []
          },
          {
            "level": 3,
            "title": "1.3 状态机（连接）",
            "slug": "_1-3-状态机-连接",
            "link": "#_1-3-状态机-连接",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "2. Transport 层",
        "slug": "_2-transport-层",
        "link": "#_2-transport-层",
        "children": [
          {
            "level": 3,
            "title": "2.1 类图",
            "slug": "_2-1-类图",
            "link": "#_2-1-类图",
            "children": []
          },
          {
            "level": 3,
            "title": "2.2 接口详情",
            "slug": "_2-2-接口详情",
            "link": "#_2-2-接口详情",
            "children": []
          },
          {
            "level": 3,
            "title": "2.3 交互序列（Dial）",
            "slug": "_2-3-交互序列-dial",
            "link": "#_2-3-交互序列-dial",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "3. AOI Service",
        "slug": "_3-aoi-service",
        "link": "#_3-aoi-service",
        "children": [
          {
            "level": 3,
            "title": "3.1 类图",
            "slug": "_3-1-类图",
            "link": "#_3-1-类图",
            "children": []
          },
          {
            "level": 3,
            "title": "3.2 核心接口",
            "slug": "_3-2-核心接口",
            "link": "#_3-2-核心接口",
            "children": []
          },
          {
            "level": 3,
            "title": "3.3 状态流程",
            "slug": "_3-3-状态流程",
            "link": "#_3-3-状态流程",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "4. DataProxy",
        "slug": "_4-dataproxy",
        "link": "#_4-dataproxy",
        "children": [
          {
            "level": 3,
            "title": "4.1 类图",
            "slug": "_4-1-类图",
            "link": "#_4-1-类图",
            "children": []
          },
          {
            "level": 3,
            "title": "4.2 主要接口",
            "slug": "_4-2-主要接口",
            "link": "#_4-2-主要接口",
            "children": []
          },
          {
            "level": 3,
            "title": "4.3 序列图（LoadPlayer）",
            "slug": "_4-3-序列图-loadplayer",
            "link": "#_4-3-序列图-loadplayer",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "5. Battle Service",
        "slug": "_5-battle-service",
        "link": "#_5-battle-service",
        "children": [
          {
            "level": 3,
            "title": "5.1 类图",
            "slug": "_5-1-类图",
            "link": "#_5-1-类图",
            "children": []
          },
          {
            "level": 3,
            "title": "5.2 关键接口",
            "slug": "_5-2-关键接口",
            "link": "#_5-2-关键接口",
            "children": []
          },
          {
            "level": 3,
            "title": "5.3 状态机（Battle Instance）",
            "slug": "_5-3-状态机-battle-instance",
            "link": "#_5-3-状态机-battle-instance",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "6. 插件框架",
        "slug": "_6-插件框架",
        "link": "#_6-插件框架",
        "children": [
          {
            "level": 3,
            "title": "6.1 类图",
            "slug": "_6-1-类图",
            "link": "#_6-1-类图",
            "children": []
          },
          {
            "level": 3,
            "title": "6.2 接口",
            "slug": "_6-2-接口",
            "link": "#_6-2-接口",
            "children": []
          },
          {
            "level": 3,
            "title": "6.3 加载流程",
            "slug": "_6-3-加载流程",
            "link": "#_6-3-加载流程",
            "children": []
          }
        ]
      }
    ],
    "path": "/27-Module_Interface_Spec.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "测试与质量保障策略",
    "headers": [
      {
        "level": 2,
        "title": "1. 测试维度概览",
        "slug": "_1-测试维度概览",
        "link": "#_1-测试维度概览",
        "children": []
      },
      {
        "level": 2,
        "title": "2. 单元测试",
        "slug": "_2-单元测试",
        "link": "#_2-单元测试",
        "children": [
          {
            "level": 3,
            "title": "2.1 范围",
            "slug": "_2-1-范围",
            "link": "#_2-1-范围",
            "children": []
          },
          {
            "level": 3,
            "title": "2.2 策略",
            "slug": "_2-2-策略",
            "link": "#_2-2-策略",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "3. 组件/模块测试",
        "slug": "_3-组件-模块测试",
        "link": "#_3-组件-模块测试",
        "children": [
          {
            "level": 3,
            "title": "NetCore",
            "slug": "netcore",
            "link": "#netcore",
            "children": []
          },
          {
            "level": 3,
            "title": "AOI",
            "slug": "aoi",
            "link": "#aoi",
            "children": []
          },
          {
            "level": 3,
            "title": "DataProxy",
            "slug": "dataproxy",
            "link": "#dataproxy",
            "children": []
          },
          {
            "level": 3,
            "title": "Battle Service",
            "slug": "battle-service",
            "link": "#battle-service",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "4. 集成测试",
        "slug": "_4-集成测试",
        "link": "#_4-集成测试",
        "children": [
          {
            "level": 3,
            "title": "4.1 环境",
            "slug": "_4-1-环境",
            "link": "#_4-1-环境",
            "children": []
          },
          {
            "level": 3,
            "title": "4.2 场景",
            "slug": "_4-2-场景",
            "link": "#_4-2-场景",
            "children": []
          },
          {
            "level": 3,
            "title": "4.3 自动化",
            "slug": "_4-3-自动化",
            "link": "#_4-3-自动化",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "5. 端到端测试 (E2E)",
        "slug": "_5-端到端测试-e2e",
        "link": "#_5-端到端测试-e2e",
        "children": [
          {
            "level": 3,
            "title": "5.1 Unity 自动化",
            "slug": "_5-1-unity-自动化",
            "link": "#_5-1-unity-自动化",
            "children": []
          },
          {
            "level": 3,
            "title": "5.2 脚本化客户端",
            "slug": "_5-2-脚本化客户端",
            "link": "#_5-2-脚本化客户端",
            "children": []
          },
          {
            "level": 3,
            "title": "5.3 数据校验",
            "slug": "_5-3-数据校验",
            "link": "#_5-3-数据校验",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "6. 回归与发布验证",
        "slug": "_6-回归与发布验证",
        "link": "#_6-回归与发布验证",
        "children": [
          {
            "level": 3,
            "title": "6.1 回放脚本",
            "slug": "_6-1-回放脚本",
            "link": "#_6-1-回放脚本",
            "children": []
          },
          {
            "level": 3,
            "title": "6.2 数据一致性",
            "slug": "_6-2-数据一致性",
            "link": "#_6-2-数据一致性",
            "children": []
          },
          {
            "level": 3,
            "title": "6.3 灰度验证",
            "slug": "_6-3-灰度验证",
            "link": "#_6-3-灰度验证",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "7. 自动化流水线",
        "slug": "_7-自动化流水线",
        "link": "#_7-自动化流水线",
        "children": [
          {
            "level": 3,
            "title": "7.1 CI 阶段",
            "slug": "_7-1-ci-阶段",
            "link": "#_7-1-ci-阶段",
            "children": []
          },
          {
            "level": 3,
            "title": "7.2 CD 阶段",
            "slug": "_7-2-cd-阶段",
            "link": "#_7-2-cd-阶段",
            "children": []
          },
          {
            "level": 3,
            "title": "7.3 工具",
            "slug": "_7-3-工具",
            "link": "#_7-3-工具",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "8. 缺陷管理与质量门禁",
        "slug": "_8-缺陷管理与质量门禁",
        "link": "#_8-缺陷管理与质量门禁",
        "children": []
      },
      {
        "level": 2,
        "title": "9. 测试数据与 Mock",
        "slug": "_9-测试数据与-mock",
        "link": "#_9-测试数据与-mock",
        "children": []
      },
      {
        "level": 2,
        "title": "10. Roadmap",
        "slug": "_10-roadmap",
        "link": "#_10-roadmap",
        "children": []
      }
    ],
    "path": "/28-Testing_Strategy.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "部署与基础设施架构",
    "headers": [
      {
        "level": 2,
        "title": "1. 环境划分",
        "slug": "_1-环境划分",
        "link": "#_1-环境划分",
        "children": []
      },
      {
        "level": 2,
        "title": "2. 基础设施",
        "slug": "_2-基础设施",
        "link": "#_2-基础设施",
        "children": [
          {
            "level": 3,
            "title": "2.1 调度/编排",
            "slug": "_2-1-调度-编排",
            "link": "#_2-1-调度-编排",
            "children": []
          },
          {
            "level": 3,
            "title": "2.2 服务治理",
            "slug": "_2-2-服务治理",
            "link": "#_2-2-服务治理",
            "children": []
          },
          {
            "level": 3,
            "title": "2.3 网络拓扑",
            "slug": "_2-3-网络拓扑",
            "link": "#_2-3-网络拓扑",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "3. 服务部署拓扑",
        "slug": "_3-服务部署拓扑",
        "link": "#_3-服务部署拓扑",
        "children": []
      },
      {
        "level": 2,
        "title": "4. 配置与 Secrets",
        "slug": "_4-配置与-secrets",
        "link": "#_4-配置与-secrets",
        "children": [
          {
            "level": 3,
            "title": "4.1 配置管理",
            "slug": "_4-1-配置管理",
            "link": "#_4-1-配置管理",
            "children": []
          },
          {
            "level": 3,
            "title": "4.2 配置升级",
            "slug": "_4-2-配置升级",
            "link": "#_4-2-配置升级",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "5. CI/CD 流程",
        "slug": "_5-ci-cd-流程",
        "link": "#_5-ci-cd-流程",
        "children": [
          {
            "level": 3,
            "title": "5.1 Pipeline 步骤",
            "slug": "_5-1-pipeline-步骤",
            "link": "#_5-1-pipeline-步骤",
            "children": []
          },
          {
            "level": 3,
            "title": "5.2 发布策略",
            "slug": "_5-2-发布策略",
            "link": "#_5-2-发布策略",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "6. 监控与日志（与 docs/20 关联）",
        "slug": "_6-监控与日志-与-docs-20-关联",
        "link": "#_6-监控与日志-与-docs-20-关联",
        "children": []
      },
      {
        "level": 2,
        "title": "7. 容灾与备份",
        "slug": "_7-容灾与备份",
        "link": "#_7-容灾与备份",
        "children": []
      },
      {
        "level": 2,
        "title": "8. 资源管理",
        "slug": "_8-资源管理",
        "link": "#_8-资源管理",
        "children": []
      },
      {
        "level": 2,
        "title": "9. 安全",
        "slug": "_9-安全",
        "link": "#_9-安全",
        "children": []
      },
      {
        "level": 2,
        "title": "10. Roadmap",
        "slug": "_10-roadmap",
        "link": "#_10-roadmap",
        "children": []
      }
    ],
    "path": "/29-Deployment_Architecture.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "Compact GameServer 设计方案（塔防/固定地图适配）",
    "headers": [
      {
        "level": 2,
        "title": "1. 场景与需求",
        "slug": "_1-场景与需求",
        "link": "#_1-场景与需求",
        "children": []
      },
      {
        "level": 2,
        "title": "2. 架构对比",
        "slug": "_2-架构对比",
        "link": "#_2-架构对比",
        "children": []
      },
      {
        "level": 2,
        "title": "3. 模块合并策略",
        "slug": "_3-模块合并策略",
        "link": "#_3-模块合并策略",
        "children": []
      },
      {
        "level": 2,
        "title": "4. Compact GameServer 结构",
        "slug": "_4-compact-gameserver-结构",
        "link": "#_4-compact-gameserver-结构",
        "children": []
      },
      {
        "level": 2,
        "title": "5. 线程与扩展",
        "slug": "_5-线程与扩展",
        "link": "#_5-线程与扩展",
        "children": []
      },
      {
        "level": 2,
        "title": "6. 数据和配置",
        "slug": "_6-数据和配置",
        "link": "#_6-数据和配置",
        "children": []
      },
      {
        "level": 2,
        "title": "7. 对 Gate/SDK 的影响",
        "slug": "_7-对-gate-sdk-的影响",
        "link": "#_7-对-gate-sdk-的影响",
        "children": []
      },
      {
        "level": 2,
        "title": "8. 迁移与升级",
        "slug": "_8-迁移与升级",
        "link": "#_8-迁移与升级",
        "children": []
      },
      {
        "level": 2,
        "title": "9. 监控与运维",
        "slug": "_9-监控与运维",
        "link": "#_9-监控与运维",
        "children": []
      },
      {
        "level": 2,
        "title": "10. Roadmap",
        "slug": "_10-roadmap",
        "link": "#_10-roadmap",
        "children": []
      }
    ],
    "path": "/30-Compact_GameServer_Design.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "Unity SDK 设计与接口规范",
    "headers": [
      {
        "level": 2,
        "title": "1. 模块结构",
        "slug": "_1-模块结构",
        "link": "#_1-模块结构",
        "children": []
      },
      {
        "level": 2,
        "title": "2. 初始化流程",
        "slug": "_2-初始化流程",
        "link": "#_2-初始化流程",
        "children": []
      },
      {
        "level": 2,
        "title": "3. 核心 API",
        "slug": "_3-核心-api",
        "link": "#_3-核心-api",
        "children": [
          {
            "level": 3,
            "title": "3.1 ApolloClient",
            "slug": "_3-1-apolloclient",
            "link": "#_3-1-apolloclient",
            "children": []
          },
          {
            "level": 3,
            "title": "3.2 AuthManager",
            "slug": "_3-2-authmanager",
            "link": "#_3-2-authmanager",
            "children": []
          },
          {
            "level": 3,
            "title": "3.3 PatchManager",
            "slug": "_3-3-patchmanager",
            "link": "#_3-3-patchmanager",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "4. 消息路由与 Protobuf",
        "slug": "_4-消息路由与-protobuf",
        "link": "#_4-消息路由与-protobuf",
        "children": []
      },
      {
        "level": 2,
        "title": "5. 连接与重连策略",
        "slug": "_5-连接与重连策略",
        "link": "#_5-连接与重连策略",
        "children": []
      },
      {
        "level": 2,
        "title": "6. 安全",
        "slug": "_6-安全",
        "link": "#_6-安全",
        "children": []
      },
      {
        "level": 2,
        "title": "7. 调试与诊断",
        "slug": "_7-调试与诊断",
        "link": "#_7-调试与诊断",
        "children": []
      },
      {
        "level": 2,
        "title": "8. 示例与文档",
        "slug": "_8-示例与文档",
        "link": "#_8-示例与文档",
        "children": []
      },
      {
        "level": 2,
        "title": "9. 发布与版本",
        "slug": "_9-发布与版本",
        "link": "#_9-发布与版本",
        "children": []
      },
      {
        "level": 2,
        "title": "10. Roadmap",
        "slug": "_10-roadmap",
        "link": "#_10-roadmap",
        "children": []
      }
    ],
    "path": "/31-Unity_SDK_Design.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "Coding & Contribution Guidelines",
    "headers": [
      {
        "level": 2,
        "title": "1. 分支与版本管理",
        "slug": "_1-分支与版本管理",
        "link": "#_1-分支与版本管理",
        "children": [
          {
            "level": 3,
            "title": "1.1 主分支策略",
            "slug": "_1-1-主分支策略",
            "link": "#_1-1-主分支策略",
            "children": []
          },
          {
            "level": 3,
            "title": "1.2 提交流程",
            "slug": "_1-2-提交流程",
            "link": "#_1-2-提交流程",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "2. 编码规范",
        "slug": "_2-编码规范",
        "link": "#_2-编码规范",
        "children": [
          {
            "level": 3,
            "title": "2.1 C++ 风格",
            "slug": "_2-1-c-风格",
            "link": "#_2-1-c-风格",
            "children": []
          },
          {
            "level": 3,
            "title": "2.2 C#/Unity",
            "slug": "_2-2-c-unity",
            "link": "#_2-2-c-unity",
            "children": []
          },
          {
            "level": 3,
            "title": "2.3 配置与脚本",
            "slug": "_2-3-配置与脚本",
            "link": "#_2-3-配置与脚本",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "3. 文档要求",
        "slug": "_3-文档要求",
        "link": "#_3-文档要求",
        "children": []
      },
      {
        "level": 2,
        "title": "4. 测试与质量门禁",
        "slug": "_4-测试与质量门禁",
        "link": "#_4-测试与质量门禁",
        "children": []
      },
      {
        "level": 2,
        "title": "5. 代码评审",
        "slug": "_5-代码评审",
        "link": "#_5-代码评审",
        "children": []
      },
      {
        "level": 2,
        "title": "6. Issue 与任务管理",
        "slug": "_6-issue-与任务管理",
        "link": "#_6-issue-与任务管理",
        "children": []
      },
      {
        "level": 2,
        "title": "7. 风险与安全",
        "slug": "_7-风险与安全",
        "link": "#_7-风险与安全",
        "children": []
      },
      {
        "level": 2,
        "title": "8. Release 与标签",
        "slug": "_8-release-与标签",
        "link": "#_8-release-与标签",
        "children": []
      },
      {
        "level": 2,
        "title": "9. 故障处理",
        "slug": "_9-故障处理",
        "link": "#_9-故障处理",
        "children": []
      },
      {
        "level": 2,
        "title": "10. Roadmap",
        "slug": "_10-roadmap",
        "link": "#_10-roadmap",
        "children": []
      }
    ],
    "path": "/32-Coding_and_Contribution_Guidelines.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "BigWorld Compatibility Layer (Apollo)",
    "headers": [
      {
        "level": 2,
        "title": "Goal",
        "slug": "goal",
        "link": "#goal",
        "children": []
      },
      {
        "level": 2,
        "title": "Implemented APIs (current)",
        "slug": "implemented-apis-current",
        "link": "#implemented-apis-current",
        "children": [
          {
            "level": 3,
            "title": "Global facade (BigWorld::*)",
            "slug": "global-facade-bigworld",
            "link": "#global-facade-bigworld",
            "children": []
          },
          {
            "level": 3,
            "title": "Entity base class (BigWorld::Entity)",
            "slug": "entity-base-class-bigworld-entity",
            "link": "#entity-base-class-bigworld-entity",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "Semantics (important)",
        "slug": "semantics-important",
        "link": "#semantics-important",
        "children": [
          {
            "level": 3,
            "title": "Update loop",
            "slug": "update-loop",
            "link": "#update-loop",
            "children": []
          },
          {
            "level": 3,
            "title": "Timer units and behavior",
            "slug": "timer-units-and-behavior",
            "link": "#timer-units-and-behavior",
            "children": []
          },
          {
            "level": 3,
            "title": "Threading / safety",
            "slug": "threading-safety",
            "link": "#threading-safety",
            "children": []
          },
          {
            "level": 3,
            "title": "Exceptions",
            "slug": "exceptions",
            "link": "#exceptions",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "Quick start",
        "slug": "quick-start",
        "link": "#quick-start",
        "children": []
      },
      {
        "level": 2,
        "title": "Tests",
        "slug": "tests",
        "link": "#tests",
        "children": [
          {
            "level": 3,
            "title": "API coverage mapping",
            "slug": "api-coverage-mapping",
            "link": "#api-coverage-mapping",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "Not implemented yet (roadmap)",
        "slug": "not-implemented-yet-roadmap",
        "link": "#not-implemented-yet-roadmap",
        "children": []
      }
    ],
    "path": "/33-BigWorld_Compatibility.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "Apollo 模块化与 CMake 拆分落地方案",
    "headers": [
      {
        "level": 2,
        "title": "目标",
        "slug": "目标",
        "link": "#目标",
        "children": []
      },
      {
        "level": 2,
        "title": "分层原则",
        "slug": "分层原则",
        "link": "#分层原则",
        "children": [
          {
            "level": 3,
            "title": "base",
            "slug": "base",
            "link": "#base",
            "children": []
          },
          {
            "level": 3,
            "title": "core",
            "slug": "core",
            "link": "#core",
            "children": []
          },
          {
            "level": 3,
            "title": "runtime",
            "slug": "runtime",
            "link": "#runtime",
            "children": []
          },
          {
            "level": 3,
            "title": "net",
            "slug": "net",
            "link": "#net",
            "children": []
          },
          {
            "level": 3,
            "title": "data",
            "slug": "data",
            "link": "#data",
            "children": []
          },
          {
            "level": 3,
            "title": "game",
            "slug": "game",
            "link": "#game",
            "children": []
          },
          {
            "level": 3,
            "title": "starter",
            "slug": "starter",
            "link": "#starter",
            "children": []
          },
          {
            "level": 3,
            "title": "apps",
            "slug": "apps",
            "link": "#apps",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "依赖方向",
        "slug": "依赖方向",
        "link": "#依赖方向",
        "children": []
      },
      {
        "level": 2,
        "title": "目录规划",
        "slug": "目录规划",
        "link": "#目录规划",
        "children": []
      },
      {
        "level": 2,
        "title": "CMake 目标命名规则",
        "slug": "cmake-目标命名规则",
        "link": "#cmake-目标命名规则",
        "children": []
      },
      {
        "level": 2,
        "title": "顶层 CMake 演进策略",
        "slug": "顶层-cmake-演进策略",
        "link": "#顶层-cmake-演进策略",
        "children": []
      },
      {
        "level": 2,
        "title": "模块迁移顺序",
        "slug": "模块迁移顺序",
        "link": "#模块迁移顺序",
        "children": []
      },
      {
        "level": 2,
        "title": "每个阶段的验收标准",
        "slug": "每个阶段的验收标准",
        "link": "#每个阶段的验收标准",
        "children": [
          {
            "level": 3,
            "title": "第一阶段",
            "slug": "第一阶段",
            "link": "#第一阶段",
            "children": []
          },
          {
            "level": 3,
            "title": "第二阶段",
            "slug": "第二阶段",
            "link": "#第二阶段",
            "children": []
          },
          {
            "level": 3,
            "title": "第三阶段",
            "slug": "第三阶段",
            "link": "#第三阶段",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "console / terminal / cli 的归属",
        "slug": "console-terminal-cli-的归属",
        "link": "#console-terminal-cli-的归属",
        "children": []
      },
      {
        "level": 2,
        "title": "当前仓库下一步建议",
        "slug": "当前仓库下一步建议",
        "link": "#当前仓库下一步建议",
        "children": []
      }
    ],
    "path": "/34-Modularization_and_CMake_Plan.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "Apollo 模块化执行清单",
    "headers": [
      {
        "level": 2,
        "title": "Phase 1: 骨架完成",
        "slug": "phase-1-骨架完成",
        "link": "#phase-1-骨架完成",
        "children": []
      },
      {
        "level": 2,
        "title": "Phase 2: 基础模块抽取",
        "slug": "phase-2-基础模块抽取",
        "link": "#phase-2-基础模块抽取",
        "children": []
      },
      {
        "level": 2,
        "title": "Phase 3: 核心模块抽取",
        "slug": "phase-3-核心模块抽取",
        "link": "#phase-3-核心模块抽取",
        "children": []
      },
      {
        "level": 2,
        "title": "Phase 4: Runtime 抽取",
        "slug": "phase-4-runtime-抽取",
        "link": "#phase-4-runtime-抽取",
        "children": []
      },
      {
        "level": 2,
        "title": "Phase 5: Data 抽取",
        "slug": "phase-5-data-抽取",
        "link": "#phase-5-data-抽取",
        "children": []
      },
      {
        "level": 2,
        "title": "Phase 6: Net 抽取",
        "slug": "phase-6-net-抽取",
        "link": "#phase-6-net-抽取",
        "children": []
      },
      {
        "level": 2,
        "title": "Phase 7: Game 抽取",
        "slug": "phase-7-game-抽取",
        "link": "#phase-7-game-抽取",
        "children": []
      },
      {
        "level": 2,
        "title": "Phase 8: Starter 与 App 装配",
        "slug": "phase-8-starter-与-app-装配",
        "link": "#phase-8-starter-与-app-装配",
        "children": []
      },
      {
        "level": 2,
        "title": "Exit Criteria",
        "slug": "exit-criteria",
        "link": "#exit-criteria",
        "children": []
      }
    ],
    "path": "/35-Modularization_Execution_Checklist.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "AOI九宫格系统 - 高效的视野管理",
    "headers": [
      {
        "level": 2,
        "title": "目录",
        "slug": "目录",
        "link": "#目录",
        "children": []
      },
      {
        "level": 2,
        "title": "什么是AOI？",
        "slug": "什么是aoi",
        "link": "#什么是aoi",
        "children": []
      },
      {
        "level": 2,
        "title": "为什么需要AOI系统？",
        "slug": "为什么需要aoi系统",
        "link": "#为什么需要aoi系统",
        "children": [
          {
            "level": 3,
            "title": "问题场景",
            "slug": "问题场景",
            "link": "#问题场景",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "为什么九宫格是高效的？",
        "slug": "为什么九宫格是高效的",
        "link": "#为什么九宫格是高效的",
        "children": [
          {
            "level": 3,
            "title": "核心思想：空间换时间",
            "slug": "核心思想-空间换时间",
            "link": "#核心思想-空间换时间",
            "children": []
          },
          {
            "level": 3,
            "title": "九宫格原理",
            "slug": "九宫格原理",
            "link": "#九宫格原理",
            "children": []
          },
          {
            "level": 3,
            "title": "为什么高效？",
            "slug": "为什么高效",
            "link": "#为什么高效",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "怎么发现目标？（核心算法）",
        "slug": "怎么发现目标-核心算法",
        "link": "#怎么发现目标-核心算法",
        "children": [
          {
            "level": 3,
            "title": "1. 坐标 → 格子编号",
            "slug": "_1-坐标-→-格子编号",
            "link": "#_1-坐标-→-格子编号",
            "children": []
          },
          {
            "level": 3,
            "title": "2. 获取九宫格索引",
            "slug": "_2-获取九宫格索引",
            "link": "#_2-获取九宫格索引",
            "children": []
          },
          {
            "level": 3,
            "title": "3. 收集视野内实体",
            "slug": "_3-收集视野内实体",
            "link": "#_3-收集视野内实体",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "怎么去重？（关键问题）",
        "slug": "怎么去重-关键问题",
        "link": "#怎么去重-关键问题",
        "children": [
          {
            "level": 3,
            "title": "问题：九宫格边界会重复计算",
            "slug": "问题-九宫格边界会重复计算",
            "link": "#问题-九宫格边界会重复计算",
            "children": []
          },
          {
            "level": 3,
            "title": "去重方案",
            "slug": "去重方案",
            "link": "#去重方案",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "完整实现示例",
        "slug": "完整实现示例",
        "link": "#完整实现示例",
        "children": []
      },
      {
        "level": 2,
        "title": "总结",
        "slug": "总结",
        "link": "#总结",
        "children": []
      }
    ],
    "path": "/AOI%E4%B9%9D%E5%AE%AB%E6%A0%BC%E7%B3%BB%E7%BB%9F%E8%AF%A6%E8%A7%A3.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "AOI广播与消息去重",
    "headers": [
      {
        "level": 2,
        "title": "目录",
        "slug": "目录",
        "link": "#目录",
        "children": []
      },
      {
        "level": 2,
        "title": "核心问题",
        "slug": "核心问题",
        "link": "#核心问题",
        "children": []
      },
      {
        "level": 2,
        "title": "一、广播类型",
        "slug": "一、广播类型",
        "link": "#一、广播类型",
        "children": [
          {
            "level": 3,
            "title": "1. 进入广播",
            "slug": "_1-进入广播",
            "link": "#_1-进入广播",
            "children": []
          },
          {
            "level": 3,
            "title": "2. 离开广播",
            "slug": "_2-离开广播",
            "link": "#_2-离开广播",
            "children": []
          },
          {
            "level": 3,
            "title": "3. 移动广播（最复杂）",
            "slug": "_3-移动广播-最复杂",
            "link": "#_3-移动广播-最复杂",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "二、消息去重策略",
        "slug": "二、消息去重策略",
        "link": "#二、消息去重策略",
        "children": [
          {
            "level": 3,
            "title": "问题：消息风暴",
            "slug": "问题-消息风暴",
            "link": "#问题-消息风暴",
            "children": []
          },
          {
            "level": 3,
            "title": "方案1: 感兴趣列表（Interest List）",
            "slug": "方案1-感兴趣列表-interest-list",
            "link": "#方案1-感兴趣列表-interest-list",
            "children": []
          },
          {
            "level": 3,
            "title": "方案2: 九宫格交叉去重",
            "slug": "方案2-九宫格交叉去重",
            "link": "#方案2-九宫格交叉去重",
            "children": []
          },
          {
            "level": 3,
            "title": "方案3: 消息聚合（Message Batching）",
            "slug": "方案3-消息聚合-message-batching",
            "link": "#方案3-消息聚合-message-batching",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "三、完整广播实现",
        "slug": "三、完整广播实现",
        "link": "#三、完整广播实现",
        "children": []
      },
      {
        "level": 2,
        "title": "四、高性能优化技巧",
        "slug": "四、高性能优化技巧",
        "link": "#四、高性能优化技巧",
        "children": [
          {
            "level": 3,
            "title": "1. 对象池复用消息对象",
            "slug": "_1-对象池复用消息对象",
            "link": "#_1-对象池复用消息对象",
            "children": []
          },
          {
            "level": 3,
            "title": "2. 区域广播（Zone Broadcast）",
            "slug": "_2-区域广播-zone-broadcast",
            "link": "#_2-区域广播-zone-broadcast",
            "children": []
          },
          {
            "level": 3,
            "title": "3. 层次化AOI（多级九宫格）",
            "slug": "_3-层次化aoi-多级九宫格",
            "link": "#_3-层次化aoi-多级九宫格",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "五、消息格式优化",
        "slug": "五、消息格式优化",
        "link": "#五、消息格式优化",
        "children": [
          {
            "level": 3,
            "title": "压缩消息结构",
            "slug": "压缩消息结构",
            "link": "#压缩消息结构",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "总结",
        "slug": "总结",
        "link": "#总结",
        "children": []
      }
    ],
    "path": "/AOI%E5%B9%BF%E6%92%AD%E4%B8%8E%E6%B6%88%E6%81%AF%E5%8E%BB%E9%87%8D.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "BigWorld 架构深度解析",
    "headers": [
      {
        "level": 2,
        "title": "目录",
        "slug": "目录",
        "link": "#目录",
        "children": []
      },
      {
        "level": 2,
        "title": "一、什么是 BigWorld？",
        "slug": "一、什么是-bigworld",
        "link": "#一、什么是-bigworld",
        "children": []
      },
      {
        "level": 2,
        "title": "二、BigWorld 核心概念",
        "slug": "二、bigworld-核心概念",
        "link": "#二、bigworld-核心概念",
        "children": [
          {
            "level": 3,
            "title": "1. 空间分割架构 (CellApps)",
            "slug": "_1-空间分割架构-cellapps",
            "link": "#_1-空间分割架构-cellapps",
            "children": []
          },
          {
            "level": 3,
            "title": "2. 实体-组件分离 (Entity + Real/Shadow)",
            "slug": "_2-实体-组件分离-entity-real-shadow",
            "link": "#_2-实体-组件分离-entity-real-shadow",
            "children": []
          },
          {
            "level": 3,
            "title": "3. 唯一ID系统",
            "slug": "_3-唯一id系统",
            "link": "#_3-唯一id系统",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "三、BigWorld vs 传统架构对比",
        "slug": "三、bigworld-vs-传统架构对比",
        "link": "#三、bigworld-vs-传统架构对比",
        "children": [
          {
            "level": 3,
            "title": "对比表",
            "slug": "对比表",
            "link": "#对比表",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "四、BigWorld 核心组件",
        "slug": "四、bigworld-核心组件",
        "link": "#四、bigworld-核心组件",
        "children": [
          {
            "level": 3,
            "title": "1. CellApp - 游戏逻辑服务器",
            "slug": "_1-cellapp-游戏逻辑服务器",
            "link": "#_1-cellapp-游戏逻辑服务器",
            "children": []
          },
          {
            "level": 3,
            "title": "2. BaseApp - 数据库服务器",
            "slug": "_2-baseapp-数据库服务器",
            "link": "#_2-baseapp-数据库服务器",
            "children": []
          },
          {
            "level": 3,
            "title": "3. LoginApp - 登录服务器",
            "slug": "_3-loginapp-登录服务器",
            "link": "#_3-loginapp-登录服务器",
            "children": []
          },
          {
            "level": 3,
            "title": "4. Proxy - 玩家连接代理",
            "slug": "_4-proxy-玩家连接代理",
            "link": "#_4-proxy-玩家连接代理",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "五、BigWorld 的特点",
        "slug": "五、bigworld-的特点",
        "link": "#五、bigworld-的特点",
        "children": [
          {
            "level": 3,
            "title": "✅ 优点",
            "slug": "✅-优点",
            "link": "#✅-优点",
            "children": []
          },
          {
            "level": 3,
            "title": "❌ 缺点",
            "slug": "❌-缺点",
            "link": "#❌-缺点",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "六、CellApp 水平扩展详解",
        "slug": "六、cellapp-水平扩展详解",
        "link": "#六、cellapp-水平扩展详解",
        "children": [
          {
            "level": 3,
            "title": "核心思想：按位置（空间）拆分",
            "slug": "核心思想-按位置-空间-拆分",
            "link": "#核心思想-按位置-空间-拆分",
            "children": []
          },
          {
            "level": 3,
            "title": "传统 vs BigWorld 扩展方式对比",
            "slug": "传统-vs-bigworld-扩展方式对比",
            "link": "#传统-vs-bigworld-扩展方式对比",
            "children": []
          },
          {
            "level": 3,
            "title": "动态负载均衡",
            "slug": "动态负载均衡",
            "link": "#动态负载均衡",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "七、跨边界处理",
        "slug": "七、跨边界处理",
        "link": "#七、跨边界处理",
        "children": [
          {
            "level": 3,
            "title": "1. 玩家跨 CellApp 移动",
            "slug": "_1-玩家跨-cellapp-移动",
            "link": "#_1-玩家跨-cellapp-移动",
            "children": []
          },
          {
            "level": 3,
            "title": "2. 边界交互",
            "slug": "_2-边界交互",
            "link": "#_2-边界交互",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "八、适用场景",
        "slug": "八、适用场景",
        "link": "#八、适用场景",
        "children": [
          {
            "level": 3,
            "title": "BigWorld 适合的场景 ✅",
            "slug": "bigworld-适合的场景-✅",
            "link": "#bigworld-适合的场景-✅",
            "children": []
          },
          {
            "level": 3,
            "title": "BigWorld 不适合的场景 ❌",
            "slug": "bigworld-不适合的场景-❌",
            "link": "#bigworld-不适合的场景-❌",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "九、BigWorld 在 Apollo 中的兼容层",
        "slug": "九、bigworld-在-apollo-中的兼容层",
        "link": "#九、bigworld-在-apollo-中的兼容层",
        "children": []
      },
      {
        "level": 2,
        "title": "十、总结",
        "slug": "十、总结",
        "link": "#十、总结",
        "children": []
      }
    ],
    "path": "/BigWorld%E6%9E%B6%E6%9E%84%E6%B7%B1%E5%BA%A6%E8%A7%A3%E6%9E%90.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "BigWorld 进程架构与玩家生命周期",
    "headers": [
      {
        "level": 2,
        "title": "目录",
        "slug": "目录",
        "link": "#目录",
        "children": []
      },
      {
        "level": 2,
        "title": "BigWorld 进程架构图",
        "slug": "bigworld-进程架构图",
        "link": "#bigworld-进程架构图",
        "children": [
          {
            "level": 3,
            "title": "整体架构",
            "slug": "整体架构",
            "link": "#整体架构",
            "children": []
          },
          {
            "level": 3,
            "title": "进程职责总结",
            "slug": "进程职责总结",
            "link": "#进程职责总结",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "进程间通信关系",
        "slug": "进程间通信关系",
        "link": "#进程间通信关系",
        "children": [
          {
            "level": 3,
            "title": "通信拓扑图",
            "slug": "通信拓扑图",
            "link": "#通信拓扑图",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "玩家登录流程",
        "slug": "玩家登录流程",
        "link": "#玩家登录流程",
        "children": [
          {
            "level": 3,
            "title": "流程图",
            "slug": "流程图",
            "link": "#流程图",
            "children": []
          },
          {
            "level": 3,
            "title": "代码示例",
            "slug": "代码示例",
            "link": "#代码示例",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "玩家游戏流程",
        "slug": "玩家游戏流程",
        "link": "#玩家游戏流程",
        "children": [
          {
            "level": 3,
            "title": "游戏主循环消息流",
            "slug": "游戏主循环消息流",
            "link": "#游戏主循环消息流",
            "children": []
          },
          {
            "level": 3,
            "title": "CellApp 内部处理",
            "slug": "cellapp-内部处理",
            "link": "#cellapp-内部处理",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "玩家对战流程",
        "slug": "玩家对战流程",
        "link": "#玩家对战流程",
        "children": [
          {
            "level": 3,
            "title": "战斗消息流",
            "slug": "战斗消息流",
            "link": "#战斗消息流",
            "children": []
          },
          {
            "level": 3,
            "title": "跨CellApp战斗",
            "slug": "跨cellapp战斗",
            "link": "#跨cellapp战斗",
            "children": []
          },
          {
            "level": 3,
            "title": "战斗代码示例",
            "slug": "战斗代码示例",
            "link": "#战斗代码示例",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "玩家聊天流程",
        "slug": "玩家聊天流程",
        "link": "#玩家聊天流程",
        "children": [
          {
            "level": 3,
            "title": "聊天消息流",
            "slug": "聊天消息流",
            "link": "#聊天消息流",
            "children": []
          },
          {
            "level": 3,
            "title": "聊天频道类型",
            "slug": "聊天频道类型",
            "link": "#聊天频道类型",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "玩家下线流程",
        "slug": "玩家下线流程",
        "link": "#玩家下线流程",
        "children": [
          {
            "level": 3,
            "title": "正常下线",
            "slug": "正常下线",
            "link": "#正常下线",
            "children": []
          },
          {
            "level": 3,
            "title": "异常掉线处理",
            "slug": "异常掉线处理",
            "link": "#异常掉线处理",
            "children": []
          },
          {
            "level": 3,
            "title": "下线代码示例",
            "slug": "下线代码示例",
            "link": "#下线代码示例",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "完整玩家生命周期图",
        "slug": "完整玩家生命周期图",
        "link": "#完整玩家生命周期图",
        "children": []
      },
      {
        "level": 2,
        "title": "总结",
        "slug": "总结",
        "link": "#总结",
        "children": [
          {
            "level": 3,
            "title": "进程间协作模式",
            "slug": "进程间协作模式",
            "link": "#进程间协作模式",
            "children": []
          },
          {
            "level": 3,
            "title": "关键设计要点",
            "slug": "关键设计要点",
            "link": "#关键设计要点",
            "children": []
          }
        ]
      }
    ],
    "path": "/BigWorld%E8%BF%9B%E7%A8%8B%E6%9E%B6%E6%9E%84%E4%B8%8E%E7%8E%A9%E5%AE%B6%E7%94%9F%E5%91%BD%E5%91%A8%E6%9C%9F.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "构建 Apollo MMORPG 框架",
    "headers": [
      {
        "level": 2,
        "title": "使用 vcpkg（推荐）",
        "slug": "使用-vcpkg-推荐",
        "link": "#使用-vcpkg-推荐",
        "children": [
          {
            "level": 3,
            "title": "1. 安装 vcpkg",
            "slug": "_1-安装-vcpkg",
            "link": "#_1-安装-vcpkg",
            "children": []
          },
          {
            "level": 3,
            "title": "2. 构建项目",
            "slug": "_2-构建项目",
            "link": "#_2-构建项目",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "使用系统包管理器",
        "slug": "使用系统包管理器",
        "link": "#使用系统包管理器",
        "children": [
          {
            "level": 3,
            "title": "Ubuntu/Debian",
            "slug": "ubuntu-debian",
            "link": "#ubuntu-debian",
            "children": []
          },
          {
            "level": 3,
            "title": "macOS",
            "slug": "macos",
            "link": "#macos",
            "children": []
          },
          {
            "level": 3,
            "title": "Windows",
            "slug": "windows",
            "link": "#windows",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "构建选项",
        "slug": "构建选项",
        "link": "#构建选项",
        "children": [
          {
            "level": 3,
            "title": "CMake 选项",
            "slug": "cmake-选项",
            "link": "#cmake-选项",
            "children": []
          },
          {
            "level": 3,
            "title": "vcpkg Features",
            "slug": "vcpkg-features",
            "link": "#vcpkg-features",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "运行测试",
        "slug": "运行测试",
        "link": "#运行测试",
        "children": []
      },
      {
        "level": 2,
        "title": "生成文档",
        "slug": "生成文档",
        "link": "#生成文档",
        "children": []
      },
      {
        "level": 2,
        "title": "开发者指南",
        "slug": "开发者指南",
        "link": "#开发者指南",
        "children": [
          {
            "level": 3,
            "title": "添加新的依赖",
            "slug": "添加新的依赖",
            "link": "#添加新的依赖",
            "children": []
          },
          {
            "level": 3,
            "title": "Windows 开发提示",
            "slug": "windows-开发提示",
            "link": "#windows-开发提示",
            "children": []
          },
          {
            "level": 3,
            "title": "Linux 开发提示",
            "slug": "linux-开发提示",
            "link": "#linux-开发提示",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "故障排除",
        "slug": "故障排除",
        "link": "#故障排除",
        "children": [
          {
            "level": 3,
            "title": "找不到依赖",
            "slug": "找不到依赖",
            "link": "#找不到依赖",
            "children": []
          },
          {
            "level": 3,
            "title": "Windows 特定问题",
            "slug": "windows-特定问题",
            "link": "#windows-特定问题",
            "children": []
          },
          {
            "level": 3,
            "title": "macOS 特定问题",
            "slug": "macos-特定问题",
            "link": "#macos-特定问题",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "性能优化建议",
        "slug": "性能优化建议",
        "link": "#性能优化建议",
        "children": []
      }
    ],
    "path": "/BUILDING.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "CAF (C++ Actor Framework) 集成指南",
    "headers": [
      {
        "level": 2,
        "title": "为什么选择 CAF？",
        "slug": "为什么选择-caf",
        "link": "#为什么选择-caf",
        "children": []
      },
      {
        "level": 2,
        "title": "安装 CAF",
        "slug": "安装-caf",
        "link": "#安装-caf",
        "children": [
          {
            "level": 3,
            "title": "使用 vcpkg (推荐)",
            "slug": "使用-vcpkg-推荐",
            "link": "#使用-vcpkg-推荐",
            "children": []
          },
          {
            "level": 3,
            "title": "使用 conan",
            "slug": "使用-conan",
            "link": "#使用-conan",
            "children": []
          },
          {
            "level": 3,
            "title": "从源码编译",
            "slug": "从源码编译",
            "link": "#从源码编译",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "CMake 集成",
        "slug": "cmake-集成",
        "link": "#cmake-集成",
        "children": []
      },
      {
        "level": 2,
        "title": "基本 CAF Actor 示例",
        "slug": "基本-caf-actor-示例",
        "link": "#基本-caf-actor-示例",
        "children": []
      },
      {
        "level": 2,
        "title": "CAF 核心概念",
        "slug": "caf-核心概念",
        "link": "#caf-核心概念",
        "children": [
          {
            "level": 3,
            "title": "1. Actor 基类",
            "slug": "_1-actor-基类",
            "link": "#_1-actor-基类",
            "children": []
          },
          {
            "level": 3,
            "title": "2. 消息发送",
            "slug": "_2-消息发送",
            "link": "#_2-消息发送",
            "children": []
          },
          {
            "level": 3,
            "title": "3. Actor 监督",
            "slug": "_3-actor-监督",
            "link": "#_3-actor-监督",
            "children": []
          },
          {
            "level": 3,
            "title": "4. 分布式 Actor",
            "slug": "_4-分布式-actor",
            "link": "#_4-分布式-actor",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "迁移指南",
        "slug": "迁移指南",
        "link": "#迁移指南",
        "children": [
          {
            "level": 3,
            "title": "从 Apollo Actor 迁移到 CAF",
            "slug": "从-apollo-actor-迁移到-caf",
            "link": "#从-apollo-actor-迁移到-caf",
            "children": []
          },
          {
            "level": 3,
            "title": "代码对比",
            "slug": "代码对比",
            "link": "#代码对比",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "参考资源",
        "slug": "参考资源",
        "link": "#参考资源",
        "children": []
      }
    ],
    "path": "/caf_integration.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "Apollo 项目目录结构",
    "headers": [
      {
        "level": 2,
        "title": "概述",
        "slug": "概述",
        "link": "#概述",
        "children": []
      },
      {
        "level": 2,
        "title": "目录结构设计原则",
        "slug": "目录结构设计原则",
        "link": "#目录结构设计原则",
        "children": []
      },
      {
        "level": 2,
        "title": "推荐目录结构",
        "slug": "推荐目录结构",
        "link": "#推荐目录结构",
        "children": []
      },
      {
        "level": 2,
        "title": "目录说明",
        "slug": "目录说明",
        "link": "#目录说明",
        "children": [
          {
            "level": 3,
            "title": "framework/",
            "slug": "framework",
            "link": "#framework",
            "children": []
          },
          {
            "level": 3,
            "title": "game/",
            "slug": "game",
            "link": "#game",
            "children": []
          },
          {
            "level": 3,
            "title": "network/",
            "slug": "network",
            "link": "#network",
            "children": []
          },
          {
            "level": 3,
            "title": "storage/",
            "slug": "storage",
            "link": "#storage",
            "children": []
          },
          {
            "level": 3,
            "title": "server/",
            "slug": "server",
            "link": "#server",
            "children": []
          },
          {
            "level": 3,
            "title": "utils/",
            "slug": "utils",
            "link": "#utils",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "命名规范",
        "slug": "命名规范",
        "link": "#命名规范",
        "children": []
      },
      {
        "level": 2,
        "title": "文件组织原则",
        "slug": "文件组织原则",
        "link": "#文件组织原则",
        "children": []
      },
      {
        "level": 2,
        "title": "注意事项",
        "slug": "注意事项",
        "link": "#注意事项",
        "children": []
      },
      {
        "level": 2,
        "title": "最佳实践",
        "slug": "最佳实践",
        "link": "#最佳实践",
        "children": []
      }
    ],
    "path": "/Directory_Structure.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "Server → Apollo 框架抽取工作总结",
    "headers": [
      {
        "level": 2,
        "title": "概述",
        "slug": "概述",
        "link": "#概述",
        "children": []
      },
      {
        "level": 2,
        "title": "已完成的抽取工作",
        "slug": "已完成的抽取工作",
        "link": "#已完成的抽取工作",
        "children": [
          {
            "level": 3,
            "title": "1. 属性系统 (apollo/game/attributes/)",
            "slug": "_1-属性系统-apollo-game-attributes",
            "link": "#_1-属性系统-apollo-game-attributes",
            "children": []
          },
          {
            "level": 3,
            "title": "2. 配置加载器 (apollo/config/)",
            "slug": "_2-配置加载器-apollo-config",
            "link": "#_2-配置加载器-apollo-config",
            "children": []
          },
          {
            "level": 3,
            "title": "3. 网络抽象层 (apollo/net/)",
            "slug": "_3-网络抽象层-apollo-net",
            "link": "#_3-网络抽象层-apollo-net",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "目录结构",
        "slug": "目录结构",
        "link": "#目录结构",
        "children": []
      },
      {
        "level": 2,
        "title": "待抽取的模块",
        "slug": "待抽取的模块",
        "link": "#待抽取的模块",
        "children": [
          {
            "level": 3,
            "title": "高优先级",
            "slug": "高优先级",
            "link": "#高优先级",
            "children": []
          },
          {
            "level": 3,
            "title": "中优先级",
            "slug": "中优先级",
            "link": "#中优先级",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "命名规范",
        "slug": "命名规范",
        "link": "#命名规范",
        "children": [
          {
            "level": 3,
            "title": "C++ 命名约定",
            "slug": "c-命名约定",
            "link": "#c-命名约定",
            "children": []
          },
          {
            "level": 3,
            "title": "中文拼音 → 英文转换",
            "slug": "中文拼音-→-英文转换",
            "link": "#中文拼音-→-英文转换",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "使用示例",
        "slug": "使用示例",
        "link": "#使用示例",
        "children": [
          {
            "level": 3,
            "title": "属性容器使用",
            "slug": "属性容器使用",
            "link": "#属性容器使用",
            "children": []
          },
          {
            "level": 3,
            "title": "配置加载使用",
            "slug": "配置加载使用",
            "link": "#配置加载使用",
            "children": []
          },
          {
            "level": 3,
            "title": "网络层使用",
            "slug": "网络层使用",
            "link": "#网络层使用",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "下一步计划",
        "slug": "下一步计划",
        "link": "#下一步计划",
        "children": []
      }
    ],
    "path": "/extraction-progress.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "Apollo 网络层设计文档",
    "headers": [
      {
        "level": 2,
        "title": "1. 概述",
        "slug": "_1-概述",
        "link": "#_1-概述",
        "children": [
          {
            "level": 3,
            "title": "1.1 设计目标",
            "slug": "_1-1-设计目标",
            "link": "#_1-1-设计目标",
            "children": []
          },
          {
            "level": 3,
            "title": "1.2 支持的网络后端",
            "slug": "_1-2-支持的网络后端",
            "link": "#_1-2-支持的网络后端",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "2. 架构设计",
        "slug": "_2-架构设计",
        "link": "#_2-架构设计",
        "children": []
      },
      {
        "level": 2,
        "title": "3. 核心接口",
        "slug": "_3-核心接口",
        "link": "#_3-核心接口",
        "children": [
          {
            "level": 3,
            "title": "3.1 Connection (连接接口)",
            "slug": "_3-1-connection-连接接口",
            "link": "#_3-1-connection-连接接口",
            "children": []
          },
          {
            "level": 3,
            "title": "3.2 Session (会话接口)",
            "slug": "_3-2-session-会话接口",
            "link": "#_3-2-session-会话接口",
            "children": []
          },
          {
            "level": 3,
            "title": "3.3 Listener (监听器接口)",
            "slug": "_3-3-listener-监听器接口",
            "link": "#_3-3-listener-监听器接口",
            "children": []
          },
          {
            "level": 3,
            "title": "3.4 Connector (连接器接口)",
            "slug": "_3-4-connector-连接器接口",
            "link": "#_3-4-connector-连接器接口",
            "children": []
          },
          {
            "level": 3,
            "title": "3.5 PacketParser (数据包解析器接口)",
            "slug": "_3-5-packetparser-数据包解析器接口",
            "link": "#_3-5-packetparser-数据包解析器接口",
            "children": []
          },
          {
            "level": 3,
            "title": "3.6 NetworkManager (网络管理器)",
            "slug": "_3-6-networkmanager-网络管理器",
            "link": "#_3-6-networkmanager-网络管理器",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "4. SSEngine/SDNet 适配",
        "slug": "_4-ssengine-sdnet-适配",
        "link": "#_4-ssengine-sdnet-适配",
        "children": [
          {
            "level": 3,
            "title": "4.1 接口映射",
            "slug": "_4-1-接口映射",
            "link": "#_4-1-接口映射",
            "children": []
          },
          {
            "level": 3,
            "title": "4.2 使用方式",
            "slug": "_4-2-使用方式",
            "link": "#_4-2-使用方式",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "5. 数据包格式",
        "slug": "_5-数据包格式",
        "link": "#_5-数据包格式",
        "children": [
          {
            "level": 3,
            "title": "5.1 默认格式",
            "slug": "_5-1-默认格式",
            "link": "#_5-1-默认格式",
            "children": []
          },
          {
            "level": 3,
            "title": "5.2 自定义解析器",
            "slug": "_5-2-自定义解析器",
            "link": "#_5-2-自定义解析器",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "6. 线程模型",
        "slug": "_6-线程模型",
        "link": "#_6-线程模型",
        "children": [
          {
            "level": 3,
            "title": "6.1 SSEngine/SDNet",
            "slug": "_6-1-ssengine-sdnet",
            "link": "#_6-1-ssengine-sdnet",
            "children": []
          },
          {
            "level": 3,
            "title": "6.2 Boost.Asio",
            "slug": "_6-2-boost-asio",
            "link": "#_6-2-boost-asio",
            "children": []
          },
          {
            "level": 3,
            "title": "6.3 libuv",
            "slug": "_6-3-libuv",
            "link": "#_6-3-libuv",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "7. 性能考虑",
        "slug": "_7-性能考虑",
        "link": "#_7-性能考虑",
        "children": []
      },
      {
        "level": 2,
        "title": "8. 迁移指南",
        "slug": "_8-迁移指南",
        "link": "#_8-迁移指南",
        "children": [
          {
            "level": 3,
            "title": "8.1 从 SSEngine 迁移",
            "slug": "_8-1-从-ssengine-迁移",
            "link": "#_8-1-从-ssengine-迁移",
            "children": []
          },
          {
            "level": 3,
            "title": "8.2 切换网络后端",
            "slug": "_8-2-切换网络后端",
            "link": "#_8-2-切换网络后端",
            "children": []
          }
        ]
      }
    ],
    "path": "/network-design.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "MMORPG 架构 QA",
    "headers": [
      {
        "level": 2,
        "title": "一、架构设计篇",
        "slug": "一、架构设计篇",
        "link": "#一、架构设计篇",
        "children": []
      },
      {
        "level": 2,
        "title": "二、网络通信篇",
        "slug": "二、网络通信篇",
        "link": "#二、网络通信篇",
        "children": []
      },
      {
        "level": 2,
        "title": "三、数据存储篇",
        "slug": "三、数据存储篇",
        "link": "#三、数据存储篇",
        "children": []
      },
      {
        "level": 2,
        "title": "四、游戏逻辑篇",
        "slug": "四、游戏逻辑篇",
        "link": "#四、游戏逻辑篇",
        "children": []
      },
      {
        "level": 2,
        "title": "五、性能优化篇",
        "slug": "五、性能优化篇",
        "link": "#五、性能优化篇",
        "children": []
      },
      {
        "level": 2,
        "title": "六、并发与多线程篇",
        "slug": "六、并发与多线程篇",
        "link": "#六、并发与多线程篇",
        "children": []
      },
      {
        "level": 2,
        "title": "七、安全篇",
        "slug": "七、安全篇",
        "link": "#七、安全篇",
        "children": []
      },
      {
        "level": 2,
        "title": "八、运维与监控篇",
        "slug": "八、运维与监控篇",
        "link": "#八、运维与监控篇",
        "children": []
      },
      {
        "level": 2,
        "title": "九、脚本系统篇",
        "slug": "九、脚本系统篇",
        "link": "#九、脚本系统篇",
        "children": []
      },
      {
        "level": 2,
        "title": "十、实战经验篇",
        "slug": "十、实战经验篇",
        "link": "#十、实战经验篇",
        "children": []
      }
    ],
    "path": "/QA.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "Apollo",
    "headers": [
      {
        "level": 2,
        "title": "简介",
        "slug": "简介",
        "link": "#简介",
        "children": []
      },
      {
        "level": 2,
        "title": "核心特性",
        "slug": "核心特性",
        "link": "#核心特性",
        "children": [
          {
            "level": 3,
            "title": "分层模块架构",
            "slug": "分层模块架构",
            "link": "#分层模块架构",
            "children": []
          },
          {
            "level": 3,
            "title": "BigWorld 架构支持",
            "slug": "bigworld-架构支持",
            "link": "#bigworld-架构支持",
            "children": []
          },
          {
            "level": 3,
            "title": "AOI 九宫格系统",
            "slug": "aoi-九宫格系统",
            "link": "#aoi-九宫格系统",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "快速开始",
        "slug": "快速开始",
        "link": "#快速开始",
        "children": []
      },
      {
        "level": 2,
        "title": "文档导航",
        "slug": "文档导航",
        "link": "#文档导航",
        "children": []
      },
      {
        "level": 2,
        "title": "社区",
        "slug": "社区",
        "link": "#社区",
        "children": []
      },
      {
        "level": 2,
        "title": "许可证",
        "slug": "许可证",
        "link": "#许可证",
        "children": []
      }
    ],
    "path": "/",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "TODO 任务计划",
    "headers": [
      {
        "level": 2,
        "title": "组件补充计划",
        "slug": "组件补充计划",
        "link": "#组件补充计划",
        "children": [
          {
            "level": 3,
            "title": "高优先级",
            "slug": "高优先级",
            "link": "#高优先级",
            "children": []
          },
          {
            "level": 3,
            "title": "中优先级",
            "slug": "中优先级",
            "link": "#中优先级",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "功能补充计划",
        "slug": "功能补充计划",
        "link": "#功能补充计划",
        "children": [
          {
            "level": 3,
            "title": "核心 MMO 功能",
            "slug": "核心-mmo-功能",
            "link": "#核心-mmo-功能",
            "children": []
          },
          {
            "level": 3,
            "title": "客户端 SDK",
            "slug": "客户端-sdk",
            "link": "#客户端-sdk",
            "children": []
          },
          {
            "level": 3,
            "title": "脚本系统",
            "slug": "脚本系统",
            "link": "#脚本系统",
            "children": []
          },
          {
            "level": 3,
            "title": "监控与运维",
            "slug": "监控与运维",
            "link": "#监控与运维",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "文档完善计划",
        "slug": "文档完善计划",
        "link": "#文档完善计划",
        "children": []
      }
    ],
    "path": "/todo.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "Base API",
    "headers": [
      {
        "level": 2,
        "title": "apollo::base::Time",
        "slug": "apollo-base-time",
        "link": "#apollo-base-time",
        "children": []
      },
      {
        "level": 2,
        "title": "apollo::base::ThreadPool",
        "slug": "apollo-base-threadpool",
        "link": "#apollo-base-threadpool",
        "children": []
      },
      {
        "level": 2,
        "title": "apollo::base::IdPool",
        "slug": "apollo-base-idpool",
        "link": "#apollo-base-idpool",
        "children": []
      },
      {
        "level": 2,
        "title": "apollo::base::ObjectPool",
        "slug": "apollo-base-objectpool",
        "link": "#apollo-base-objectpool",
        "children": []
      },
      {
        "level": 2,
        "title": "apollo::base::String",
        "slug": "apollo-base-string",
        "link": "#apollo-base-string",
        "children": []
      },
      {
        "level": 2,
        "title": "apollo::base::FixedMemoryPool",
        "slug": "apollo-base-fixedmemorypool",
        "link": "#apollo-base-fixedmemorypool",
        "children": []
      }
    ],
    "path": "/api/base.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "Core API",
    "headers": [
      {
        "level": 2,
        "title": "apollo::core::Application",
        "slug": "apollo-core-application",
        "link": "#apollo-core-application",
        "children": []
      },
      {
        "level": 2,
        "title": "apollo::core::LogManager",
        "slug": "apollo-core-logmanager",
        "link": "#apollo-core-logmanager",
        "children": []
      },
      {
        "level": 2,
        "title": "日志宏",
        "slug": "日志宏",
        "link": "#日志宏",
        "children": []
      },
      {
        "level": 2,
        "title": "apollo::core::ConfigManager",
        "slug": "apollo-core-configmanager",
        "link": "#apollo-core-configmanager",
        "children": []
      },
      {
        "level": 2,
        "title": "apollo::core::ApplicationContext",
        "slug": "apollo-core-applicationcontext",
        "link": "#apollo-core-applicationcontext",
        "children": []
      },
      {
        "level": 2,
        "title": "apollo::core::EventBus",
        "slug": "apollo-core-eventbus",
        "link": "#apollo-core-eventbus",
        "children": []
      }
    ],
    "path": "/api/core.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "Data API",
    "headers": [
      {
        "level": 2,
        "title": "apollo::data::orm::Session",
        "slug": "apollo-data-orm-session",
        "link": "#apollo-data-orm-session",
        "children": []
      },
      {
        "level": 2,
        "title": "apollo::data::orm::Query",
        "slug": "apollo-data-orm-query",
        "link": "#apollo-data-orm-query",
        "children": []
      },
      {
        "level": 2,
        "title": "apollo::data::orm::Entity",
        "slug": "apollo-data-orm-entity",
        "link": "#apollo-data-orm-entity",
        "children": []
      },
      {
        "level": 2,
        "title": "apollo::data::orm::ConnectionPool",
        "slug": "apollo-data-orm-connectionpool",
        "link": "#apollo-data-orm-connectionpool",
        "children": []
      },
      {
        "level": 2,
        "title": "apollo::data::redis::RedisClient",
        "slug": "apollo-data-redis-redisclient",
        "link": "#apollo-data-redis-redisclient",
        "children": []
      },
      {
        "level": 2,
        "title": "apollo::data::cache::Cache",
        "slug": "apollo-data-cache-cache",
        "link": "#apollo-data-cache-cache",
        "children": []
      }
    ],
    "path": "/api/data.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "Game API",
    "headers": [
      {
        "level": 2,
        "title": "apollo::game::Entity",
        "slug": "apollo-game-entity",
        "link": "#apollo-game-entity",
        "children": []
      },
      {
        "level": 2,
        "title": "apollo::game::Component",
        "slug": "apollo-game-component",
        "link": "#apollo-game-component",
        "children": []
      },
      {
        "level": 2,
        "title": "apollo::game::world::AOIManager",
        "slug": "apollo-game-world-aoimanager",
        "link": "#apollo-game-world-aoimanager",
        "children": []
      },
      {
        "level": 2,
        "title": "apollo::game::battle::ECSWorld",
        "slug": "apollo-game-battle-ecsworld",
        "link": "#apollo-game-battle-ecsworld",
        "children": []
      },
      {
        "level": 2,
        "title": "apollo::game::battle::ECSSystem",
        "slug": "apollo-game-battle-ecssystem",
        "link": "#apollo-game-battle-ecssystem",
        "children": []
      },
      {
        "level": 2,
        "title": "apollo::game::attributes::AttributeManager",
        "slug": "apollo-game-attributes-attributemanager",
        "link": "#apollo-game-attributes-attributemanager",
        "children": []
      },
      {
        "level": 2,
        "title": "apollo::game::attributes::AttributeType",
        "slug": "apollo-game-attributes-attributetype",
        "link": "#apollo-game-attributes-attributetype",
        "children": []
      },
      {
        "level": 2,
        "title": "apollo::game::attributes::AttributeModifier",
        "slug": "apollo-game-attributes-attributemodifier",
        "link": "#apollo-game-attributes-attributemodifier",
        "children": []
      }
    ],
    "path": "/api/game.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "Network API",
    "headers": [
      {
        "level": 2,
        "title": "apollo::net::tcp::Server",
        "slug": "apollo-net-tcp-server",
        "link": "#apollo-net-tcp-server",
        "children": []
      },
      {
        "level": 2,
        "title": "apollo::net::tcp::Client",
        "slug": "apollo-net-tcp-client",
        "link": "#apollo-net-tcp-client",
        "children": []
      },
      {
        "level": 2,
        "title": "apollo::net::http::Server",
        "slug": "apollo-net-http-server",
        "link": "#apollo-net-http-server",
        "children": []
      },
      {
        "level": 2,
        "title": "apollo::net::http::Request",
        "slug": "apollo-net-http-request",
        "link": "#apollo-net-http-request",
        "children": []
      },
      {
        "level": 2,
        "title": "apollo::net::http::Response",
        "slug": "apollo-net-http-response",
        "link": "#apollo-net-http-response",
        "children": []
      },
      {
        "level": 2,
        "title": "apollo::net::http::Client",
        "slug": "apollo-net-http-client",
        "link": "#apollo-net-http-client",
        "children": []
      },
      {
        "level": 2,
        "title": "apollo::net::ws::Server",
        "slug": "apollo-net-ws-server",
        "link": "#apollo-net-ws-server",
        "children": []
      },
      {
        "level": 2,
        "title": "apollo::net::ws::Connection",
        "slug": "apollo-net-ws-connection",
        "link": "#apollo-net-ws-connection",
        "children": []
      }
    ],
    "path": "/api/net.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "API 参考",
    "headers": [
      {
        "level": 2,
        "title": "模块 API",
        "slug": "模块-api",
        "link": "#模块-api",
        "children": []
      },
      {
        "level": 2,
        "title": "命名空间",
        "slug": "命名空间",
        "link": "#命名空间",
        "children": []
      },
      {
        "level": 2,
        "title": "错误处理",
        "slug": "错误处理",
        "link": "#错误处理",
        "children": []
      },
      {
        "level": 2,
        "title": "线程安全",
        "slug": "线程安全",
        "link": "#线程安全",
        "children": []
      }
    ],
    "path": "/api/",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "Runtime API",
    "headers": [
      {
        "level": 2,
        "title": "apollo::runtime::ApplicationHost",
        "slug": "apollo-runtime-applicationhost",
        "link": "#apollo-runtime-applicationhost",
        "children": []
      },
      {
        "level": 2,
        "title": "apollo::runtime::Console",
        "slug": "apollo-runtime-console",
        "link": "#apollo-runtime-console",
        "children": []
      },
      {
        "level": 2,
        "title": "apollo::runtime::Signal",
        "slug": "apollo-runtime-signal",
        "link": "#apollo-runtime-signal",
        "children": []
      },
      {
        "level": 2,
        "title": "apollo::runtime::HealthCheck",
        "slug": "apollo-runtime-healthcheck",
        "link": "#apollo-runtime-healthcheck",
        "children": []
      }
    ],
    "path": "/api/runtime.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "BigWorld 服务器应用实现",
    "headers": [
      {
        "level": 2,
        "title": "目录",
        "slug": "目录",
        "link": "#目录",
        "children": []
      },
      {
        "level": 2,
        "title": "架构概览",
        "slug": "架构概览",
        "link": "#架构概览",
        "children": [
          {
            "level": 3,
            "title": "服务器拓扑",
            "slug": "服务器拓扑",
            "link": "#服务器拓扑",
            "children": []
          },
          {
            "level": 3,
            "title": "进程职责",
            "slug": "进程职责",
            "link": "#进程职责",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "通信协议",
        "slug": "通信协议",
        "link": "#通信协议",
        "children": [
          {
            "level": 3,
            "title": "NNG (nanomsg-next-gen)",
            "slug": "nng-nanomsg-next-gen",
            "link": "#nng-nanomsg-next-gen",
            "children": []
          },
          {
            "level": 3,
            "title": "消息格式",
            "slug": "消息格式",
            "link": "#消息格式",
            "children": []
          },
          {
            "level": 3,
            "title": "消息类型",
            "slug": "消息类型",
            "link": "#消息类型",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "GatewayApp 网关服务器",
        "slug": "gatewayapp-网关服务器",
        "link": "#gatewayapp-网关服务器",
        "children": [
          {
            "level": 3,
            "title": "职责",
            "slug": "职责",
            "link": "#职责",
            "children": []
          },
          {
            "level": 3,
            "title": "使用示例",
            "slug": "使用示例",
            "link": "#使用示例",
            "children": []
          },
          {
            "level": 3,
            "title": "目录结构",
            "slug": "目录结构",
            "link": "#目录结构",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "LoginApp 登录服务器",
        "slug": "loginapp-登录服务器",
        "link": "#loginapp-登录服务器",
        "children": [
          {
            "level": 3,
            "title": "职责",
            "slug": "职责-1",
            "link": "#职责-1",
            "children": []
          },
          {
            "level": 3,
            "title": "登录流程",
            "slug": "登录流程",
            "link": "#登录流程",
            "children": []
          },
          {
            "level": 3,
            "title": "使用示例",
            "slug": "使用示例-1",
            "link": "#使用示例-1",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "BaseApp 数据库服务器",
        "slug": "baseapp-数据库服务器",
        "link": "#baseapp-数据库服务器",
        "children": [
          {
            "level": 3,
            "title": "职责",
            "slug": "职责-2",
            "link": "#职责-2",
            "children": []
          },
          {
            "level": 3,
            "title": "玩家数据结构",
            "slug": "玩家数据结构",
            "link": "#玩家数据结构",
            "children": []
          },
          {
            "level": 3,
            "title": "使用示例",
            "slug": "使用示例-2",
            "link": "#使用示例-2",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "CellApp 游戏逻辑服务器",
        "slug": "cellapp-游戏逻辑服务器",
        "link": "#cellapp-游戏逻辑服务器",
        "children": [
          {
            "level": 3,
            "title": "职责",
            "slug": "职责-3",
            "link": "#职责-3",
            "children": []
          },
          {
            "level": 3,
            "title": "实体类型",
            "slug": "实体类型",
            "link": "#实体类型",
            "children": []
          },
          {
            "level": 3,
            "title": "使用示例",
            "slug": "使用示例-3",
            "link": "#使用示例-3",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "部署方式",
        "slug": "部署方式",
        "link": "#部署方式",
        "children": [
          {
            "level": 3,
            "title": "单机部署（开发）",
            "slug": "单机部署-开发",
            "link": "#单机部署-开发",
            "children": []
          },
          {
            "level": 3,
            "title": "分布式部署（生产）",
            "slug": "分布式部署-生产",
            "link": "#分布式部署-生产",
            "children": []
          },
          {
            "level": 3,
            "title": "Docker Compose 部署",
            "slug": "docker-compose-部署",
            "link": "#docker-compose-部署",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "运行指南",
        "slug": "运行指南",
        "link": "#运行指南",
        "children": [
          {
            "level": 3,
            "title": "构建项目",
            "slug": "构建项目",
            "link": "#构建项目",
            "children": []
          },
          {
            "level": 3,
            "title": "启动服务器",
            "slug": "启动服务器",
            "link": "#启动服务器",
            "children": []
          },
          {
            "level": 3,
            "title": "启动顺序",
            "slug": "启动顺序",
            "link": "#启动顺序",
            "children": []
          },
          {
            "level": 3,
            "title": "关闭顺序",
            "slug": "关闭顺序",
            "link": "#关闭顺序",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "总结",
        "slug": "总结",
        "link": "#总结",
        "children": []
      },
      {
        "level": 2,
        "title": "相关文档",
        "slug": "相关文档",
        "link": "#相关文档",
        "children": []
      }
    ],
    "path": "/apps/BigWorld%E6%9C%8D%E5%8A%A1%E5%99%A8%E5%BA%94%E7%94%A8%E5%AE%9E%E7%8E%B0.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "服务器应用",
    "headers": [
      {
        "level": 2,
        "title": "应用列表",
        "slug": "应用列表",
        "link": "#应用列表",
        "children": []
      },
      {
        "level": 2,
        "title": "相关文档",
        "slug": "相关文档",
        "link": "#相关文档",
        "children": []
      }
    ],
    "path": "/apps/",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "AOI广播与消息去重",
    "headers": [
      {
        "level": 2,
        "title": "目录",
        "slug": "目录",
        "link": "#目录",
        "children": []
      },
      {
        "level": 2,
        "title": "核心问题",
        "slug": "核心问题",
        "link": "#核心问题",
        "children": []
      },
      {
        "level": 2,
        "title": "一、广播类型",
        "slug": "一、广播类型",
        "link": "#一、广播类型",
        "children": [
          {
            "level": 3,
            "title": "1. 进入广播",
            "slug": "_1-进入广播",
            "link": "#_1-进入广播",
            "children": []
          },
          {
            "level": 3,
            "title": "2. 离开广播",
            "slug": "_2-离开广播",
            "link": "#_2-离开广播",
            "children": []
          },
          {
            "level": 3,
            "title": "3. 移动广播（最复杂）",
            "slug": "_3-移动广播-最复杂",
            "link": "#_3-移动广播-最复杂",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "二、消息去重策略",
        "slug": "二、消息去重策略",
        "link": "#二、消息去重策略",
        "children": [
          {
            "level": 3,
            "title": "问题：消息风暴",
            "slug": "问题-消息风暴",
            "link": "#问题-消息风暴",
            "children": []
          },
          {
            "level": 3,
            "title": "方案1: 感兴趣列表（Interest List）",
            "slug": "方案1-感兴趣列表-interest-list",
            "link": "#方案1-感兴趣列表-interest-list",
            "children": []
          },
          {
            "level": 3,
            "title": "方案2: 九宫格交叉去重",
            "slug": "方案2-九宫格交叉去重",
            "link": "#方案2-九宫格交叉去重",
            "children": []
          },
          {
            "level": 3,
            "title": "方案3: 消息聚合（Message Batching）",
            "slug": "方案3-消息聚合-message-batching",
            "link": "#方案3-消息聚合-message-batching",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "三、完整广播实现",
        "slug": "三、完整广播实现",
        "link": "#三、完整广播实现",
        "children": []
      },
      {
        "level": 2,
        "title": "四、高性能优化技巧",
        "slug": "四、高性能优化技巧",
        "link": "#四、高性能优化技巧",
        "children": [
          {
            "level": 3,
            "title": "1. 对象池复用消息对象",
            "slug": "_1-对象池复用消息对象",
            "link": "#_1-对象池复用消息对象",
            "children": []
          },
          {
            "level": 3,
            "title": "2. 区域广播（Zone Broadcast）",
            "slug": "_2-区域广播-zone-broadcast",
            "link": "#_2-区域广播-zone-broadcast",
            "children": []
          },
          {
            "level": 3,
            "title": "3. 层次化AOI（多级九宫格）",
            "slug": "_3-层次化aoi-多级九宫格",
            "link": "#_3-层次化aoi-多级九宫格",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "五、消息格式优化",
        "slug": "五、消息格式优化",
        "link": "#五、消息格式优化",
        "children": [
          {
            "level": 3,
            "title": "压缩消息结构",
            "slug": "压缩消息结构",
            "link": "#压缩消息结构",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "总结",
        "slug": "总结",
        "link": "#总结",
        "children": []
      }
    ],
    "path": "/architecture/aoi-broadcast.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "AOI九宫格系统详解",
    "headers": [
      {
        "level": 2,
        "title": "目录",
        "slug": "目录",
        "link": "#目录",
        "children": []
      },
      {
        "level": 2,
        "title": "什么是AOI？",
        "slug": "什么是aoi",
        "link": "#什么是aoi",
        "children": []
      },
      {
        "level": 2,
        "title": "为什么需要AOI系统？",
        "slug": "为什么需要aoi系统",
        "link": "#为什么需要aoi系统",
        "children": [
          {
            "level": 3,
            "title": "问题场景",
            "slug": "问题场景",
            "link": "#问题场景",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "为什么九宫格是高效的？",
        "slug": "为什么九宫格是高效的",
        "link": "#为什么九宫格是高效的",
        "children": [
          {
            "level": 3,
            "title": "核心思想：空间换时间",
            "slug": "核心思想-空间换时间",
            "link": "#核心思想-空间换时间",
            "children": []
          },
          {
            "level": 3,
            "title": "九宫格原理",
            "slug": "九宫格原理",
            "link": "#九宫格原理",
            "children": []
          },
          {
            "level": 3,
            "title": "为什么高效？",
            "slug": "为什么高效",
            "link": "#为什么高效",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "怎么发现目标？（核心算法）",
        "slug": "怎么发现目标-核心算法",
        "link": "#怎么发现目标-核心算法",
        "children": [
          {
            "level": 3,
            "title": "1. 坐标 → 格子编号",
            "slug": "_1-坐标-→-格子编号",
            "link": "#_1-坐标-→-格子编号",
            "children": []
          },
          {
            "level": 3,
            "title": "2. 获取九宫格索引",
            "slug": "_2-获取九宫格索引",
            "link": "#_2-获取九宫格索引",
            "children": []
          },
          {
            "level": 3,
            "title": "3. 收集视野内实体",
            "slug": "_3-收集视野内实体",
            "link": "#_3-收集视野内实体",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "怎么去重？（关键问题）",
        "slug": "怎么去重-关键问题",
        "link": "#怎么去重-关键问题",
        "children": [
          {
            "level": 3,
            "title": "问题：九宫格边界会重复计算",
            "slug": "问题-九宫格边界会重复计算",
            "link": "#问题-九宫格边界会重复计算",
            "children": []
          },
          {
            "level": 3,
            "title": "去重方案",
            "slug": "去重方案",
            "link": "#去重方案",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "完整实现示例",
        "slug": "完整实现示例",
        "link": "#完整实现示例",
        "children": []
      },
      {
        "level": 2,
        "title": "总结",
        "slug": "总结",
        "link": "#总结",
        "children": []
      }
    ],
    "path": "/architecture/aoi.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "BigWorld 进程架构与玩家生命周期",
    "headers": [
      {
        "level": 2,
        "title": "目录",
        "slug": "目录",
        "link": "#目录",
        "children": []
      },
      {
        "level": 2,
        "title": "BigWorld 进程架构图",
        "slug": "bigworld-进程架构图",
        "link": "#bigworld-进程架构图",
        "children": [
          {
            "level": 3,
            "title": "整体架构",
            "slug": "整体架构",
            "link": "#整体架构",
            "children": []
          },
          {
            "level": 3,
            "title": "进程职责总结",
            "slug": "进程职责总结",
            "link": "#进程职责总结",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "进程间通信关系",
        "slug": "进程间通信关系",
        "link": "#进程间通信关系",
        "children": [
          {
            "level": 3,
            "title": "通信拓扑图",
            "slug": "通信拓扑图",
            "link": "#通信拓扑图",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "玩家登录流程",
        "slug": "玩家登录流程",
        "link": "#玩家登录流程",
        "children": [
          {
            "level": 3,
            "title": "流程图",
            "slug": "流程图",
            "link": "#流程图",
            "children": []
          },
          {
            "level": 3,
            "title": "代码示例",
            "slug": "代码示例",
            "link": "#代码示例",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "玩家游戏流程",
        "slug": "玩家游戏流程",
        "link": "#玩家游戏流程",
        "children": [
          {
            "level": 3,
            "title": "游戏主循环消息流",
            "slug": "游戏主循环消息流",
            "link": "#游戏主循环消息流",
            "children": []
          },
          {
            "level": 3,
            "title": "CellApp 内部处理",
            "slug": "cellapp-内部处理",
            "link": "#cellapp-内部处理",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "玩家对战流程",
        "slug": "玩家对战流程",
        "link": "#玩家对战流程",
        "children": [
          {
            "level": 3,
            "title": "战斗消息流",
            "slug": "战斗消息流",
            "link": "#战斗消息流",
            "children": []
          },
          {
            "level": 3,
            "title": "跨CellApp战斗",
            "slug": "跨cellapp战斗",
            "link": "#跨cellapp战斗",
            "children": []
          },
          {
            "level": 3,
            "title": "战斗代码示例",
            "slug": "战斗代码示例",
            "link": "#战斗代码示例",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "玩家聊天流程",
        "slug": "玩家聊天流程",
        "link": "#玩家聊天流程",
        "children": [
          {
            "level": 3,
            "title": "聊天消息流",
            "slug": "聊天消息流",
            "link": "#聊天消息流",
            "children": []
          },
          {
            "level": 3,
            "title": "聊天频道类型",
            "slug": "聊天频道类型",
            "link": "#聊天频道类型",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "玩家下线流程",
        "slug": "玩家下线流程",
        "link": "#玩家下线流程",
        "children": [
          {
            "level": 3,
            "title": "正常下线",
            "slug": "正常下线",
            "link": "#正常下线",
            "children": []
          },
          {
            "level": 3,
            "title": "异常掉线处理",
            "slug": "异常掉线处理",
            "link": "#异常掉线处理",
            "children": []
          },
          {
            "level": 3,
            "title": "下线代码示例",
            "slug": "下线代码示例",
            "link": "#下线代码示例",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "完整玩家生命周期图",
        "slug": "完整玩家生命周期图",
        "link": "#完整玩家生命周期图",
        "children": []
      },
      {
        "level": 2,
        "title": "总结",
        "slug": "总结",
        "link": "#总结",
        "children": [
          {
            "level": 3,
            "title": "进程间协作模式",
            "slug": "进程间协作模式",
            "link": "#进程间协作模式",
            "children": []
          },
          {
            "level": 3,
            "title": "关键设计要点",
            "slug": "关键设计要点",
            "link": "#关键设计要点",
            "children": []
          }
        ]
      }
    ],
    "path": "/architecture/bigworld-lifecycle.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "BigWorld 架构深度解析",
    "headers": [
      {
        "level": 2,
        "title": "目录",
        "slug": "目录",
        "link": "#目录",
        "children": []
      },
      {
        "level": 2,
        "title": "一、什么是 BigWorld？",
        "slug": "一、什么是-bigworld",
        "link": "#一、什么是-bigworld",
        "children": []
      },
      {
        "level": 2,
        "title": "二、BigWorld 核心概念",
        "slug": "二、bigworld-核心概念",
        "link": "#二、bigworld-核心概念",
        "children": [
          {
            "level": 3,
            "title": "1. 空间分割架构 (CellApps)",
            "slug": "_1-空间分割架构-cellapps",
            "link": "#_1-空间分割架构-cellapps",
            "children": []
          },
          {
            "level": 3,
            "title": "2. 实体-组件分离 (Entity + Real/Shadow)",
            "slug": "_2-实体-组件分离-entity-real-shadow",
            "link": "#_2-实体-组件分离-entity-real-shadow",
            "children": []
          },
          {
            "level": 3,
            "title": "3. 唯一ID系统",
            "slug": "_3-唯一id系统",
            "link": "#_3-唯一id系统",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "三、BigWorld vs 传统架构对比",
        "slug": "三、bigworld-vs-传统架构对比",
        "link": "#三、bigworld-vs-传统架构对比",
        "children": [
          {
            "level": 3,
            "title": "对比表",
            "slug": "对比表",
            "link": "#对比表",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "四、BigWorld 核心组件",
        "slug": "四、bigworld-核心组件",
        "link": "#四、bigworld-核心组件",
        "children": [
          {
            "level": 3,
            "title": "1. CellApp - 游戏逻辑服务器",
            "slug": "_1-cellapp-游戏逻辑服务器",
            "link": "#_1-cellapp-游戏逻辑服务器",
            "children": []
          },
          {
            "level": 3,
            "title": "2. BaseApp - 数据库服务器",
            "slug": "_2-baseapp-数据库服务器",
            "link": "#_2-baseapp-数据库服务器",
            "children": []
          },
          {
            "level": 3,
            "title": "3. LoginApp - 登录服务器",
            "slug": "_3-loginapp-登录服务器",
            "link": "#_3-loginapp-登录服务器",
            "children": []
          },
          {
            "level": 3,
            "title": "4. Proxy - 玩家连接代理",
            "slug": "_4-proxy-玩家连接代理",
            "link": "#_4-proxy-玩家连接代理",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "五、BigWorld 的特点",
        "slug": "五、bigworld-的特点",
        "link": "#五、bigworld-的特点",
        "children": [
          {
            "level": 3,
            "title": "✅ 优点",
            "slug": "✅-优点",
            "link": "#✅-优点",
            "children": []
          },
          {
            "level": 3,
            "title": "❌ 缺点",
            "slug": "❌-缺点",
            "link": "#❌-缺点",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "六、CellApp 水平扩展详解",
        "slug": "六、cellapp-水平扩展详解",
        "link": "#六、cellapp-水平扩展详解",
        "children": [
          {
            "level": 3,
            "title": "核心思想：按位置（空间）拆分",
            "slug": "核心思想-按位置-空间-拆分",
            "link": "#核心思想-按位置-空间-拆分",
            "children": []
          },
          {
            "level": 3,
            "title": "传统 vs BigWorld 扩展方式对比",
            "slug": "传统-vs-bigworld-扩展方式对比",
            "link": "#传统-vs-bigworld-扩展方式对比",
            "children": []
          },
          {
            "level": 3,
            "title": "动态负载均衡",
            "slug": "动态负载均衡",
            "link": "#动态负载均衡",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "七、跨边界处理",
        "slug": "七、跨边界处理",
        "link": "#七、跨边界处理",
        "children": [
          {
            "level": 3,
            "title": "1. 玩家跨 CellApp 移动",
            "slug": "_1-玩家跨-cellapp-移动",
            "link": "#_1-玩家跨-cellapp-移动",
            "children": []
          },
          {
            "level": 3,
            "title": "2. 边界交互",
            "slug": "_2-边界交互",
            "link": "#_2-边界交互",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "八、适用场景",
        "slug": "八、适用场景",
        "link": "#八、适用场景",
        "children": [
          {
            "level": 3,
            "title": "BigWorld 适合的场景 ✅",
            "slug": "bigworld-适合的场景-✅",
            "link": "#bigworld-适合的场景-✅",
            "children": []
          },
          {
            "level": 3,
            "title": "BigWorld 不适合的场景 ❌",
            "slug": "bigworld-不适合的场景-❌",
            "link": "#bigworld-不适合的场景-❌",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "九、BigWorld 在 Apollo 中的兼容层",
        "slug": "九、bigworld-在-apollo-中的兼容层",
        "link": "#九、bigworld-在-apollo-中的兼容层",
        "children": []
      },
      {
        "level": 2,
        "title": "十、总结",
        "slug": "十、总结",
        "link": "#十、总结",
        "children": []
      }
    ],
    "path": "/architecture/bigworld.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "架构概述",
    "headers": [
      {
        "level": 2,
        "title": "整体架构",
        "slug": "整体架构",
        "link": "#整体架构",
        "children": []
      },
      {
        "level": 2,
        "title": "层级说明",
        "slug": "层级说明",
        "link": "#层级说明",
        "children": [
          {
            "level": 3,
            "title": "Level 1: Base (modules/base/)",
            "slug": "level-1-base-modules-base",
            "link": "#level-1-base-modules-base",
            "children": []
          },
          {
            "level": 3,
            "title": "Level 2: Core (modules/core/)",
            "slug": "level-2-core-modules-core",
            "link": "#level-2-core-modules-core",
            "children": []
          },
          {
            "level": 3,
            "title": "Level 3: Runtime (modules/runtime/)",
            "slug": "level-3-runtime-modules-runtime",
            "link": "#level-3-runtime-modules-runtime",
            "children": []
          },
          {
            "level": 3,
            "title": "Level 4: Data (modules/data/)",
            "slug": "level-4-data-modules-data",
            "link": "#level-4-data-modules-data",
            "children": []
          },
          {
            "level": 3,
            "title": "Level 5: Network (modules/net/)",
            "slug": "level-5-network-modules-net",
            "link": "#level-5-network-modules-net",
            "children": []
          },
          {
            "level": 3,
            "title": "Level 6: Actor (modules/actor/)",
            "slug": "level-6-actor-modules-actor",
            "link": "#level-6-actor-modules-actor",
            "children": []
          },
          {
            "level": 3,
            "title": "Level 7: Game (modules/game/)",
            "slug": "level-7-game-modules-game",
            "link": "#level-7-game-modules-game",
            "children": []
          },
          {
            "level": 3,
            "title": "Level 8: BigWorld (modules/bigworld/)",
            "slug": "level-8-bigworld-modules-bigworld",
            "link": "#level-8-bigworld-modules-bigworld",
            "children": []
          },
          {
            "level": 3,
            "title": "Level 9: Starter (modules/starter/)",
            "slug": "level-9-starter-modules-starter",
            "link": "#level-9-starter-modules-starter",
            "children": []
          },
          {
            "level": 3,
            "title": "Level 10: Apps (apps/)",
            "slug": "level-10-apps-apps",
            "link": "#level-10-apps-apps",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "依赖关系",
        "slug": "依赖关系",
        "link": "#依赖关系",
        "children": []
      }
    ],
    "path": "/architecture/overview.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "Base 模块",
    "headers": [
      {
        "level": 2,
        "title": "组件",
        "slug": "组件",
        "link": "#组件",
        "children": [
          {
            "level": 3,
            "title": "Time",
            "slug": "time",
            "link": "#time",
            "children": []
          },
          {
            "level": 3,
            "title": "ThreadPool",
            "slug": "threadpool",
            "link": "#threadpool",
            "children": []
          },
          {
            "level": 3,
            "title": "IdPool",
            "slug": "idpool",
            "link": "#idpool",
            "children": []
          },
          {
            "level": 3,
            "title": "ObjectPool",
            "slug": "objectpool",
            "link": "#objectpool",
            "children": []
          },
          {
            "level": 3,
            "title": "String",
            "slug": "string",
            "link": "#string",
            "children": []
          },
          {
            "level": 3,
            "title": "Memory",
            "slug": "memory",
            "link": "#memory",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "依赖",
        "slug": "依赖",
        "link": "#依赖",
        "children": []
      },
      {
        "level": 2,
        "title": "链接",
        "slug": "链接",
        "link": "#链接",
        "children": []
      }
    ],
    "path": "/modules/base.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "BigWorld 模块",
    "headers": [
      {
        "level": 2,
        "title": "概述",
        "slug": "概述",
        "link": "#概述",
        "children": []
      },
      {
        "level": 2,
        "title": "基本用法",
        "slug": "基本用法",
        "link": "#基本用法",
        "children": [
          {
            "level": 3,
            "title": "实体操作",
            "slug": "实体操作",
            "link": "#实体操作",
            "children": []
          },
          {
            "level": 3,
            "title": "回调系统",
            "slug": "回调系统",
            "link": "#回调系统",
            "children": []
          },
          {
            "level": 3,
            "title": "定时器",
            "slug": "定时器",
            "link": "#定时器",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "迁移指南",
        "slug": "迁移指南",
        "link": "#迁移指南",
        "children": [
          {
            "level": 3,
            "title": "从 BigWorld 迁移",
            "slug": "从-bigworld-迁移",
            "link": "#从-bigworld-迁移",
            "children": []
          },
          {
            "level": 3,
            "title": "不兼容的部分",
            "slug": "不兼容的部分",
            "link": "#不兼容的部分",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "依赖",
        "slug": "依赖",
        "link": "#依赖",
        "children": []
      },
      {
        "level": 2,
        "title": "链接",
        "slug": "链接",
        "link": "#链接",
        "children": []
      },
      {
        "level": 2,
        "title": "相关文档",
        "slug": "相关文档",
        "link": "#相关文档",
        "children": []
      }
    ],
    "path": "/modules/bigworld.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "Core 模块",
    "headers": [
      {
        "level": 2,
        "title": "组件",
        "slug": "组件",
        "link": "#组件",
        "children": [
          {
            "level": 3,
            "title": "依赖注入 (DI)",
            "slug": "依赖注入-di",
            "link": "#依赖注入-di",
            "children": []
          },
          {
            "level": 3,
            "title": "配置 (Config)",
            "slug": "配置-config",
            "link": "#配置-config",
            "children": []
          },
          {
            "level": 3,
            "title": "日志 (Log)",
            "slug": "日志-log",
            "link": "#日志-log",
            "children": []
          },
          {
            "level": 3,
            "title": "生命周期 (Lifecycle)",
            "slug": "生命周期-lifecycle",
            "link": "#生命周期-lifecycle",
            "children": []
          },
          {
            "level": 3,
            "title": "事件 (Event)",
            "slug": "事件-event",
            "link": "#事件-event",
            "children": []
          },
          {
            "level": 3,
            "title": "定时器 (Timer)",
            "slug": "定时器-timer",
            "link": "#定时器-timer",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "依赖",
        "slug": "依赖",
        "link": "#依赖",
        "children": []
      },
      {
        "level": 2,
        "title": "链接",
        "slug": "链接",
        "link": "#链接",
        "children": []
      }
    ],
    "path": "/modules/core.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "Data 模块",
    "headers": [
      {
        "level": 2,
        "title": "ORM",
        "slug": "orm",
        "link": "#orm",
        "children": []
      },
      {
        "level": 2,
        "title": "Redis",
        "slug": "redis",
        "link": "#redis",
        "children": []
      },
      {
        "level": 2,
        "title": "连接池",
        "slug": "连接池",
        "link": "#连接池",
        "children": []
      },
      {
        "level": 2,
        "title": "缓存",
        "slug": "缓存",
        "link": "#缓存",
        "children": []
      },
      {
        "level": 2,
        "title": "依赖",
        "slug": "依赖",
        "link": "#依赖",
        "children": []
      },
      {
        "level": 2,
        "title": "链接",
        "slug": "链接",
        "link": "#链接",
        "children": []
      }
    ],
    "path": "/modules/data.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "Game 模块",
    "headers": [
      {
        "level": 2,
        "title": "Entity",
        "slug": "entity",
        "link": "#entity",
        "children": []
      },
      {
        "level": 2,
        "title": "组件系统",
        "slug": "组件系统",
        "link": "#组件系统",
        "children": []
      },
      {
        "level": 2,
        "title": "AOI 系统",
        "slug": "aoi-系统",
        "link": "#aoi-系统",
        "children": []
      },
      {
        "level": 2,
        "title": "战斗系统 (ECS)",
        "slug": "战斗系统-ecs",
        "link": "#战斗系统-ecs",
        "children": []
      },
      {
        "level": 2,
        "title": "属性系统",
        "slug": "属性系统",
        "link": "#属性系统",
        "children": []
      },
      {
        "level": 2,
        "title": "NPC AI",
        "slug": "npc-ai",
        "link": "#npc-ai",
        "children": []
      },
      {
        "level": 2,
        "title": "依赖",
        "slug": "依赖",
        "link": "#依赖",
        "children": []
      },
      {
        "level": 2,
        "title": "链接",
        "slug": "链接",
        "link": "#链接",
        "children": []
      }
    ],
    "path": "/modules/game.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "Network 模块",
    "headers": [
      {
        "level": 2,
        "title": "TCP Server",
        "slug": "tcp-server",
        "link": "#tcp-server",
        "children": []
      },
      {
        "level": 2,
        "title": "TCP Client",
        "slug": "tcp-client",
        "link": "#tcp-client",
        "children": []
      },
      {
        "level": 2,
        "title": "HTTP Server",
        "slug": "http-server",
        "link": "#http-server",
        "children": []
      },
      {
        "level": 2,
        "title": "HTTP Client",
        "slug": "http-client",
        "link": "#http-client",
        "children": []
      },
      {
        "level": 2,
        "title": "WebSocket",
        "slug": "websocket",
        "link": "#websocket",
        "children": []
      },
      {
        "level": 2,
        "title": "RPC",
        "slug": "rpc",
        "link": "#rpc",
        "children": []
      },
      {
        "level": 2,
        "title": "依赖",
        "slug": "依赖",
        "link": "#依赖",
        "children": []
      },
      {
        "level": 2,
        "title": "链接",
        "slug": "链接",
        "link": "#链接",
        "children": []
      }
    ],
    "path": "/modules/net.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "Runtime 模块",
    "headers": [
      {
        "level": 2,
        "title": "ApplicationHost",
        "slug": "applicationhost",
        "link": "#applicationhost",
        "children": []
      },
      {
        "level": 2,
        "title": "ServiceHost",
        "slug": "servicehost",
        "link": "#servicehost",
        "children": []
      },
      {
        "level": 2,
        "title": "控制台输入",
        "slug": "控制台输入",
        "link": "#控制台输入",
        "children": []
      },
      {
        "level": 2,
        "title": "信号处理",
        "slug": "信号处理",
        "link": "#信号处理",
        "children": []
      },
      {
        "level": 2,
        "title": "健康检查",
        "slug": "健康检查",
        "link": "#健康检查",
        "children": []
      },
      {
        "level": 2,
        "title": "依赖",
        "slug": "依赖",
        "link": "#依赖",
        "children": []
      },
      {
        "level": 2,
        "title": "链接",
        "slug": "链接",
        "link": "#链接",
        "children": []
      }
    ],
    "path": "/modules/runtime.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "核心概念",
    "headers": [
      {
        "level": 2,
        "title": "应用 (Application)",
        "slug": "应用-application",
        "link": "#应用-application",
        "children": []
      },
      {
        "level": 2,
        "title": "模块 (Module)",
        "slug": "模块-module",
        "link": "#模块-module",
        "children": []
      },
      {
        "level": 2,
        "title": "依赖注入 (DI)",
        "slug": "依赖注入-di",
        "link": "#依赖注入-di",
        "children": []
      },
      {
        "level": 2,
        "title": "配置 (Config)",
        "slug": "配置-config",
        "link": "#配置-config",
        "children": []
      },
      {
        "level": 2,
        "title": "日志 (Log)",
        "slug": "日志-log",
        "link": "#日志-log",
        "children": []
      },
      {
        "level": 2,
        "title": "实体 (Entity)",
        "slug": "实体-entity",
        "link": "#实体-entity",
        "children": []
      },
      {
        "level": 2,
        "title": "AOI (Area of Interest)",
        "slug": "aoi-area-of-interest",
        "link": "#aoi-area-of-interest",
        "children": []
      },
      {
        "level": 2,
        "title": "Actor 模型",
        "slug": "actor-模型",
        "link": "#actor-模型",
        "children": []
      },
      {
        "level": 2,
        "title": "下一步",
        "slug": "下一步",
        "link": "#下一步",
        "children": []
      }
    ],
    "path": "/guide/concepts.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "配置",
    "headers": [
      {
        "level": 2,
        "title": "配置文件格式",
        "slug": "配置文件格式",
        "link": "#配置文件格式",
        "children": [
          {
            "level": 3,
            "title": "config.json",
            "slug": "config-json",
            "link": "#config-json",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "使用配置",
        "slug": "使用配置",
        "link": "#使用配置",
        "children": [
          {
            "level": 3,
            "title": "加载配置",
            "slug": "加载配置",
            "link": "#加载配置",
            "children": []
          },
          {
            "level": 3,
            "title": "读取配置",
            "slug": "读取配置",
            "link": "#读取配置",
            "children": []
          },
          {
            "level": 3,
            "title": "配置监听",
            "slug": "配置监听",
            "link": "#配置监听",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "热更新",
        "slug": "热更新",
        "link": "#热更新",
        "children": []
      },
      {
        "level": 2,
        "title": "环境变量覆盖",
        "slug": "环境变量覆盖",
        "link": "#环境变量覆盖",
        "children": []
      },
      {
        "level": 2,
        "title": "命令行参数",
        "slug": "命令行参数",
        "link": "#命令行参数",
        "children": []
      },
      {
        "level": 2,
        "title": "配置验证",
        "slug": "配置验证",
        "link": "#配置验证",
        "children": []
      },
      {
        "level": 2,
        "title": "下一步",
        "slug": "下一步",
        "link": "#下一步",
        "children": []
      }
    ],
    "path": "/guide/configuration.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "安装",
    "headers": [
      {
        "level": 2,
        "title": "环境要求",
        "slug": "环境要求",
        "link": "#环境要求",
        "children": []
      },
      {
        "level": 2,
        "title": "安装步骤",
        "slug": "安装步骤",
        "link": "#安装步骤",
        "children": [
          {
            "level": 3,
            "title": "1. 安装 vcpkg",
            "slug": "_1-安装-vcpkg",
            "link": "#_1-安装-vcpkg",
            "children": []
          },
          {
            "level": 3,
            "title": "2. 安装依赖",
            "slug": "_2-安装依赖",
            "link": "#_2-安装依赖",
            "children": []
          },
          {
            "level": 3,
            "title": "3. 克隆 Apollo",
            "slug": "_3-克隆-apollo",
            "link": "#_3-克隆-apollo",
            "children": []
          },
          {
            "level": 3,
            "title": "4. 配置与构建",
            "slug": "_4-配置与构建",
            "link": "#_4-配置与构建",
            "children": []
          },
          {
            "level": 3,
            "title": "5. 验证安装",
            "slug": "_5-验证安装",
            "link": "#_5-验证安装",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "目录结构",
        "slug": "目录结构",
        "link": "#目录结构",
        "children": []
      },
      {
        "level": 2,
        "title": "下一步",
        "slug": "下一步",
        "link": "#下一步",
        "children": []
      }
    ],
    "path": "/guide/installation.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "模块系统",
    "headers": [
      {
        "level": 2,
        "title": "模块分层",
        "slug": "模块分层",
        "link": "#模块分层",
        "children": []
      },
      {
        "level": 2,
        "title": "使用模块",
        "slug": "使用模块",
        "link": "#使用模块",
        "children": [
          {
            "level": 3,
            "title": "CMakeLists.txt 配置",
            "slug": "cmakelists-txt-配置",
            "link": "#cmakelists-txt-配置",
            "children": []
          },
          {
            "level": 3,
            "title": "条件加载",
            "slug": "条件加载",
            "link": "#条件加载",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "模块依赖",
        "slug": "模块依赖",
        "link": "#模块依赖",
        "children": []
      },
      {
        "level": 2,
        "title": "自定义模块",
        "slug": "自定义模块",
        "link": "#自定义模块",
        "children": [
          {
            "level": 3,
            "title": "1. 创建模块目录",
            "slug": "_1-创建模块目录",
            "link": "#_1-创建模块目录",
            "children": []
          },
          {
            "level": 3,
            "title": "2. 编写 CMakeLists.txt",
            "slug": "_2-编写-cmakelists-txt",
            "link": "#_2-编写-cmakelists-txt",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "下一步",
        "slug": "下一步",
        "link": "#下一步",
        "children": []
      }
    ],
    "path": "/guide/module-system.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "快速开始",
    "headers": [
      {
        "level": 2,
        "title": "创建项目",
        "slug": "创建项目",
        "link": "#创建项目",
        "children": [
          {
            "level": 3,
            "title": "1. 初始化项目结构",
            "slug": "_1-初始化项目结构",
            "link": "#_1-初始化项目结构",
            "children": []
          },
          {
            "level": 3,
            "title": "2. 创建 CMakeLists.txt",
            "slug": "_2-创建-cmakelists-txt",
            "link": "#_2-创建-cmakelists-txt",
            "children": []
          },
          {
            "level": 3,
            "title": "3. 创建主程序",
            "slug": "_3-创建主程序",
            "link": "#_3-创建主程序",
            "children": []
          },
          {
            "level": 3,
            "title": "4. 构建运行",
            "slug": "_4-构建运行",
            "link": "#_4-构建运行",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "添加网络通信",
        "slug": "添加网络通信",
        "link": "#添加网络通信",
        "children": [
          {
            "level": 3,
            "title": "创建 TCP 服务器",
            "slug": "创建-tcp-服务器",
            "link": "#创建-tcp-服务器",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "下一步",
        "slug": "下一步",
        "link": "#下一步",
        "children": []
      }
    ],
    "path": "/guide/quick-start.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "指南",
    "headers": [
      {
        "level": 2,
        "title": "指南章节",
        "slug": "指南章节",
        "link": "#指南章节",
        "children": []
      },
      {
        "level": 2,
        "title": "下一步",
        "slug": "下一步",
        "link": "#下一步",
        "children": []
      }
    ],
    "path": "/guide/",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "Public Assets",
    "headers": [
      {
        "level": 2,
        "title": "文件说明",
        "slug": "文件说明",
        "link": "#文件说明",
        "children": []
      },
      {
        "level": 2,
        "title": "替换说明",
        "slug": "替换说明",
        "link": "#替换说明",
        "children": []
      }
    ],
    "path": "/public/",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "Q1: 请画出你熟悉的 MMORPG 服务器架构图，说明各组件的职责和通信方式",
    "headers": [
      {
        "level": 2,
        "title": "问题分析",
        "slug": "问题分析",
        "link": "#问题分析",
        "children": []
      },
      {
        "level": 2,
        "title": "参考架构：BigWorld/KBEngine 风格",
        "slug": "参考架构-bigworld-kbengine-风格",
        "link": "#参考架构-bigworld-kbengine-风格",
        "children": [
          {
            "level": 3,
            "title": "整体架构图",
            "slug": "整体架构图",
            "link": "#整体架构图",
            "children": []
          },
          {
            "level": 3,
            "title": "登录流程图",
            "slug": "登录流程图",
            "link": "#登录流程图",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "各组件职责详解",
        "slug": "各组件职责详解",
        "link": "#各组件职责详解",
        "children": [
          {
            "level": 3,
            "title": "1. 客户端层",
            "slug": "_1-客户端层",
            "link": "#_1-客户端层",
            "children": []
          },
          {
            "level": 3,
            "title": "2. 接入层",
            "slug": "_2-接入层",
            "link": "#_2-接入层",
            "children": []
          },
          {
            "level": 3,
            "title": "3. 登录服务",
            "slug": "_3-登录服务",
            "link": "#_3-登录服务",
            "children": []
          },
          {
            "level": 3,
            "title": "4. 管理服务层",
            "slug": "_4-管理服务层",
            "link": "#_4-管理服务层",
            "children": []
          },
          {
            "level": 3,
            "title": "5. 基础服务层",
            "slug": "_5-基础服务层",
            "link": "#_5-基础服务层",
            "children": []
          },
          {
            "level": 3,
            "title": "6. 空间服务层",
            "slug": "_6-空间服务层",
            "link": "#_6-空间服务层",
            "children": []
          },
          {
            "level": 3,
            "title": "7. 数据服务层",
            "slug": "_7-数据服务层",
            "link": "#_7-数据服务层",
            "children": []
          },
          {
            "level": 3,
            "title": "8. 其他服务",
            "slug": "_8-其他服务",
            "link": "#_8-其他服务",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "通信方式总结",
        "slug": "通信方式总结",
        "link": "#通信方式总结",
        "children": [
          {
            "level": 3,
            "title": "通信协议栈",
            "slug": "通信协议栈",
            "link": "#通信协议栈",
            "children": []
          },
          {
            "level": 3,
            "title": "消息格式",
            "slug": "消息格式",
            "link": "#消息格式",
            "children": []
          },
          {
            "level": 3,
            "title": "RPC调用流程",
            "slug": "rpc调用流程",
            "link": "#rpc调用流程",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "架构特点总结",
        "slug": "架构特点总结",
        "link": "#架构特点总结",
        "children": []
      },
      {
        "level": 2,
        "title": "参考资料",
        "slug": "参考资料",
        "link": "#参考资料",
        "children": []
      }
    ],
    "path": "/qa/q1-server-architecture.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "Q2: BigWorld 架构中的 CellApp 和 BaseApp 分别负责什么？为什么这样分离？",
    "headers": [
      {
        "level": 2,
        "title": "问题分析",
        "slug": "问题分析",
        "link": "#问题分析",
        "children": []
      },
      {
        "level": 2,
        "title": "CellApp vs BaseApp 对比",
        "slug": "cellapp-vs-baseapp-对比",
        "link": "#cellapp-vs-baseapp-对比",
        "children": [
          {
            "level": 3,
            "title": "职责对比表",
            "slug": "职责对比表",
            "link": "#职责对比表",
            "children": []
          },
          {
            "level": 3,
            "title": "详细职责",
            "slug": "详细职责",
            "link": "#详细职责",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "为什么这样分离？",
        "slug": "为什么这样分离",
        "link": "#为什么这样分离",
        "children": [
          {
            "level": 3,
            "title": "原因 1：空间与非空间逻辑的本质差异",
            "slug": "原因-1-空间与非空间逻辑的本质差异",
            "link": "#原因-1-空间与非空间逻辑的本质差异",
            "children": []
          },
          {
            "level": 3,
            "title": "原因 2：支持动态负载均衡",
            "slug": "原因-2-支持动态负载均衡",
            "link": "#原因-2-支持动态负载均衡",
            "children": []
          },
          {
            "level": 3,
            "title": "原因 3：Entity 迁移的复杂性",
            "slug": "原因-3-entity-迁移的复杂性",
            "link": "#原因-3-entity-迁移的复杂性",
            "children": []
          },
          {
            "level": 3,
            "title": "原因 4：网络通信优化",
            "slug": "原因-4-网络通信优化",
            "link": "#原因-4-网络通信优化",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "优势与挑战",
        "slug": "优势与挑战",
        "link": "#优势与挑战",
        "children": [
          {
            "level": 3,
            "title": "优势",
            "slug": "优势",
            "link": "#优势",
            "children": []
          },
          {
            "level": 3,
            "title": "挑战",
            "slug": "挑战",
            "link": "#挑战",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "数据流转示例",
        "slug": "数据流转示例",
        "link": "#数据流转示例",
        "children": [
          {
            "level": 3,
            "title": "玩家登录流程",
            "slug": "玩家登录流程",
            "link": "#玩家登录流程",
            "children": []
          },
          {
            "level": 3,
            "title": "玩家移动流程",
            "slug": "玩家移动流程",
            "link": "#玩家移动流程",
            "children": []
          },
          {
            "level": 3,
            "title": "使用背包流程",
            "slug": "使用背包流程",
            "link": "#使用背包流程",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "参考资料",
        "slug": "参考资料",
        "link": "#参考资料",
        "children": []
      }
    ],
    "path": "/qa/q2-bigworld-cell-base.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "Q26: KBEngine 消息路由机制：Gateway、Proxy、CellApp 之间如何高效转发？",
    "headers": [
      {
        "level": 2,
        "title": "问题分析",
        "slug": "问题分析",
        "link": "#问题分析",
        "children": []
      },
      {
        "level": 2,
        "title": "整体架构",
        "slug": "整体架构",
        "link": "#整体架构",
        "children": []
      },
      {
        "level": 2,
        "title": "Gateway vs Proxy 的区别",
        "slug": "gateway-vs-proxy-的区别",
        "link": "#gateway-vs-proxy-的区别",
        "children": []
      },
      {
        "level": 2,
        "title": "消息路由流程",
        "slug": "消息路由流程",
        "link": "#消息路由流程",
        "children": [
          {
            "level": 3,
            "title": "全局流程图",
            "slug": "全局流程图",
            "link": "#全局流程图",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "Proxy 路由机制详解",
        "slug": "proxy-路由机制详解",
        "link": "#proxy-路由机制详解",
        "children": [
          {
            "level": 3,
            "title": "消息路由规则",
            "slug": "消息路由规则",
            "link": "#消息路由规则",
            "children": []
          },
          {
            "level": 3,
            "title": "Entity 位置映射表",
            "slug": "entity-位置映射表",
            "link": "#entity-位置映射表",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "为什么必须通过 Proxy 转发？",
        "slug": "为什么必须通过-proxy-转发",
        "link": "#为什么必须通过-proxy-转发",
        "children": [
          {
            "level": 3,
            "title": "原因分析",
            "slug": "原因分析",
            "link": "#原因分析",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "消息\"信封\"格式详解",
        "slug": "消息-信封-格式详解",
        "link": "#消息-信封-格式详解",
        "children": [
          {
            "level": 3,
            "title": "KBEngine 网络协议帧结构",
            "slug": "kbengine-网络协议帧结构",
            "link": "#kbengine-网络协议帧结构",
            "children": []
          },
          {
            "level": 3,
            "title": "字段详细说明",
            "slug": "字段详细说明",
            "link": "#字段详细说明",
            "children": []
          },
          {
            "level": 3,
            "title": "消息类型（MsgType）枚举",
            "slug": "消息类型-msgtype-枚举",
            "link": "#消息类型-msgtype-枚举",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "多 CellApp 场景下的路由处理",
        "slug": "多-cellapp-场景下的路由处理",
        "link": "#多-cellapp-场景下的路由处理",
        "children": [
          {
            "level": 3,
            "title": "问题：玩家在多个 CellApp 的 AOI 范围内",
            "slug": "问题-玩家在多个-cellapp-的-aoi-范围内",
            "link": "#问题-玩家在多个-cellapp-的-aoi-范围内",
            "children": []
          },
          {
            "level": 3,
            "title": "解决方案：Entity 位置注册表",
            "slug": "解决方案-entity-位置注册表",
            "link": "#解决方案-entity-位置注册表",
            "children": []
          },
          {
            "level": 3,
            "title": "跨 CellApp 消息处理流程",
            "slug": "跨-cellapp-消息处理流程",
            "link": "#跨-cellapp-消息处理流程",
            "children": []
          },
          {
            "level": 3,
            "title": "分布式位置服务",
            "slug": "分布式位置服务",
            "link": "#分布式位置服务",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "完整的消息路由示例",
        "slug": "完整的消息路由示例",
        "link": "#完整的消息路由示例",
        "children": [
          {
            "level": 3,
            "title": "玩家攻击跨 CellApp 的 NPC",
            "slug": "玩家攻击跨-cellapp-的-npc",
            "link": "#玩家攻击跨-cellapp-的-npc",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "优化：热点 Entity 的路由",
        "slug": "优化-热点-entity-的路由",
        "link": "#优化-热点-entity-的路由",
        "children": [
          {
            "level": 3,
            "title": "问题：热门 NPC 被大量玩家同时访问",
            "slug": "问题-热门-npc-被大量玩家同时访问",
            "link": "#问题-热门-npc-被大量玩家同时访问",
            "children": []
          },
          {
            "level": 3,
            "title": "解决方案：请求合并与排队",
            "slug": "解决方案-请求合并与排队",
            "link": "#解决方案-请求合并与排队",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "总结：协议帧与多 CellApp 路由",
        "slug": "总结-协议帧与多-cellapp-路由",
        "link": "#总结-协议帧与多-cellapp-路由",
        "children": [
          {
            "level": 3,
            "title": "协议帧格式速查表",
            "slug": "协议帧格式速查表",
            "link": "#协议帧格式速查表",
            "children": []
          },
          {
            "level": 3,
            "title": "多 CellApp 路由流程",
            "slug": "多-cellapp-路由流程",
            "link": "#多-cellapp-路由流程",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "参考资料",
        "slug": "参考资料",
        "link": "#参考资料",
        "children": []
      },
      {
        "level": 2,
        "title": "高效路由优化",
        "slug": "高效路由优化",
        "link": "#高效路由优化",
        "children": [
          {
            "level": 3,
            "title": "1. Entity ID 编码",
            "slug": "_1-entity-id-编码",
            "link": "#_1-entity-id-编码",
            "children": []
          },
          {
            "level": 3,
            "title": "2. 长连接复用",
            "slug": "_2-长连接复用",
            "link": "#_2-长连接复用",
            "children": []
          },
          {
            "level": 3,
            "title": "3. 消息批处理",
            "slug": "_3-消息批处理",
            "link": "#_3-消息批处理",
            "children": []
          },
          {
            "level": 3,
            "title": "4. 零拷贝转发",
            "slug": "_4-零拷贝转发",
            "link": "#_4-零拷贝转发",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "对比：统一网关 vs Proxy 转发",
        "slug": "对比-统一网关-vs-proxy-转发",
        "link": "#对比-统一网关-vs-proxy-转发",
        "children": [
          {
            "level": 3,
            "title": "KBEngine 选择 Proxy 转发的原因",
            "slug": "kbengine-选择-proxy-转发的原因",
            "link": "#kbengine-选择-proxy-转发的原因",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "总结",
        "slug": "总结",
        "link": "#总结",
        "children": []
      },
      {
        "level": 2,
        "title": "参考资料",
        "slug": "参考资料-1",
        "link": "#参考资料-1",
        "children": []
      }
    ],
    "path": "/qa/q26-kbengine-message-routing.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "Q27: Real、Ghost、Shadow Entity 之间如何转换？如何高效同步？",
    "headers": [
      {
        "level": 2,
        "title": "问题分析",
        "slug": "问题分析",
        "link": "#问题分析",
        "children": []
      },
      {
        "level": 2,
        "title": "Entity 三态定义",
        "slug": "entity-三态定义",
        "link": "#entity-三态定义",
        "children": [
          {
            "level": 3,
            "title": "概念对比表",
            "slug": "概念对比表",
            "link": "#概念对比表",
            "children": []
          },
          {
            "level": 3,
            "title": "三态关系图",
            "slug": "三态关系图",
            "link": "#三态关系图",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "三种 Entity 详解",
        "slug": "三种-entity-详解",
        "link": "#三种-entity-详解",
        "children": [
          {
            "level": 3,
            "title": "Real Entity（权威实体）",
            "slug": "real-entity-权威实体",
            "link": "#real-entity-权威实体",
            "children": []
          },
          {
            "level": 3,
            "title": "Ghost Entity（影子实体）",
            "slug": "ghost-entity-影子实体",
            "link": "#ghost-entity-影子实体",
            "children": []
          },
          {
            "level": 3,
            "title": "Shadow Entity（阴影实体）",
            "slug": "shadow-entity-阴影实体",
            "link": "#shadow-entity-阴影实体",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "Entity 转换机制",
        "slug": "entity-转换机制",
        "link": "#entity-转换机制",
        "children": [
          {
            "level": 3,
            "title": "Real ↔ Ghost 转换",
            "slug": "real-↔-ghost-转换",
            "link": "#real-↔-ghost-转换",
            "children": []
          },
          {
            "level": 3,
            "title": "Real ↔ Shadow 转换",
            "slug": "real-↔-shadow-转换",
            "link": "#real-↔-shadow-转换",
            "children": []
          },
          {
            "level": 3,
            "title": "Ghost ↔ Shadow 关系",
            "slug": "ghost-↔-shadow-关系",
            "link": "#ghost-↔-shadow-关系",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "高效同步策略",
        "slug": "高效同步策略",
        "link": "#高效同步策略",
        "children": [
          {
            "level": 3,
            "title": "1. 状态同步优化",
            "slug": "_1-状态同步优化",
            "link": "#_1-状态同步优化",
            "children": []
          },
          {
            "level": 3,
            "title": "2. Ghost 同步策略",
            "slug": "_2-ghost-同步策略",
            "link": "#_2-ghost-同步策略",
            "children": []
          },
          {
            "level": 3,
            "title": "3. Shadow 同步策略",
            "slug": "_3-shadow-同步策略",
            "link": "#_3-shadow-同步策略",
            "children": []
          },
          {
            "level": 3,
            "title": "4. 带宽优化",
            "slug": "_4-带宽优化",
            "link": "#_4-带宽优化",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "转换流程图",
        "slug": "转换流程图",
        "link": "#转换流程图",
        "children": [
          {
            "level": 3,
            "title": "Entity 完整生命周期",
            "slug": "entity-完整生命周期",
            "link": "#entity-完整生命周期",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "实现示例",
        "slug": "实现示例",
        "link": "#实现示例",
        "children": [
          {
            "level": 3,
            "title": "Ghost 创建代码示例",
            "slug": "ghost-创建代码示例",
            "link": "#ghost-创建代码示例",
            "children": []
          },
          {
            "level": 3,
            "title": "Shadow 同步代码示例",
            "slug": "shadow-同步代码示例",
            "link": "#shadow-同步代码示例",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "常见问题",
        "slug": "常见问题",
        "link": "#常见问题",
        "children": [
          {
            "level": 3,
            "title": "Q1: Ghost 和 Real 数据不一致怎么办？",
            "slug": "q1-ghost-和-real-数据不一致怎么办",
            "link": "#q1-ghost-和-real-数据不一致怎么办",
            "children": []
          },
          {
            "level": 3,
            "title": "Q2: Shadow 预测错误太多怎么办？",
            "slug": "q2-shadow-预测错误太多怎么办",
            "link": "#q2-shadow-预测错误太多怎么办",
            "children": []
          },
          {
            "level": 3,
            "title": "Q3: 边界频繁切换导致大量 Ghost 创建/销毁？",
            "slug": "q3-边界频繁切换导致大量-ghost-创建-销毁",
            "link": "#q3-边界频繁切换导致大量-ghost-创建-销毁",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "参考资料",
        "slug": "参考资料",
        "link": "#参考资料",
        "children": []
      }
    ],
    "path": "/qa/q27-entity-ghost-shadow-relationship.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "Q28: KBEngine 是否存在注册中心？CellApp 如何部署和通信？Actor 模型适用吗？",
    "headers": [
      {
        "level": 2,
        "title": "问题分析",
        "slug": "问题分析",
        "link": "#问题分析",
        "children": []
      },
      {
        "level": 2,
        "title": "一、服务注册中心",
        "slug": "一、服务注册中心",
        "link": "#一、服务注册中心",
        "children": [
          {
            "level": 3,
            "title": "KBEngine 的注册机制",
            "slug": "kbengine-的注册机制",
            "link": "#kbengine-的注册机制",
            "children": []
          },
          {
            "level": 3,
            "title": "Apollo 的改进架构",
            "slug": "apollo-的改进架构",
            "link": "#apollo-的改进架构",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "二、CellApp 部署策略",
        "slug": "二、cellapp-部署策略",
        "link": "#二、cellapp-部署策略",
        "children": [
          {
            "level": 3,
            "title": "部署方式对比",
            "slug": "部署方式对比",
            "link": "#部署方式对比",
            "children": []
          },
          {
            "level": 3,
            "title": "单机多进程部署",
            "slug": "单机多进程部署",
            "link": "#单机多进程部署",
            "children": []
          },
          {
            "level": 3,
            "title": "多机分布式部署",
            "slug": "多机分布式部署",
            "link": "#多机分布式部署",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "三、CellApp 通信机制",
        "slug": "三、cellapp-通信机制",
        "link": "#三、cellapp-通信机制",
        "children": [
          {
            "level": 3,
            "title": "通信方式对比",
            "slug": "通信方式对比",
            "link": "#通信方式对比",
            "children": []
          },
          {
            "level": 3,
            "title": "NNG 通信框架",
            "slug": "nng-通信框架",
            "link": "#nng-通信框架",
            "children": []
          },
          {
            "level": 3,
            "title": "共享内存通道",
            "slug": "共享内存通道",
            "link": "#共享内存通道",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "四、Actor 模型应用",
        "slug": "四、actor-模型应用",
        "link": "#四、actor-模型应用",
        "children": [
          {
            "level": 3,
            "title": "Actor 模型与 CellApp",
            "slug": "actor-模型与-cellapp",
            "link": "#actor-模型与-cellapp",
            "children": []
          },
          {
            "level": 3,
            "title": "Actor 模型适用性分析",
            "slug": "actor-模型适用性分析",
            "link": "#actor-模型适用性分析",
            "children": []
          },
          {
            "level": 3,
            "title": "CAF 框架对比",
            "slug": "caf-框架对比",
            "link": "#caf-框架对比",
            "children": []
          },
          {
            "level": 3,
            "title": "Apollo Actor 实现",
            "slug": "apollo-actor-实现",
            "link": "#apollo-actor-实现",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "五、通信优化策略",
        "slug": "五、通信优化策略",
        "link": "#五、通信优化策略",
        "children": [
          {
            "level": 3,
            "title": "批量消息合并",
            "slug": "批量消息合并",
            "link": "#批量消息合并",
            "children": []
          },
          {
            "level": 3,
            "title": "背压控制",
            "slug": "背压控制",
            "link": "#背压控制",
            "children": []
          },
          {
            "level": 3,
            "title": "零拷贝优化",
            "slug": "零拷贝优化",
            "link": "#零拷贝优化",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "六、部署建议",
        "slug": "六、部署建议",
        "link": "#六、部署建议",
        "children": [
          {
            "level": 3,
            "title": "小规模部署（< 1000 人）",
            "slug": "小规模部署-1000-人",
            "link": "#小规模部署-1000-人",
            "children": []
          },
          {
            "level": 3,
            "title": "中等规模（1000-5000 人）",
            "slug": "中等规模-1000-5000-人",
            "link": "#中等规模-1000-5000-人",
            "children": []
          },
          {
            "level": 3,
            "title": "大规模（> 5000 人）",
            "slug": "大规模-5000-人",
            "link": "#大规模-5000-人",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "七、参考资料",
        "slug": "七、参考资料",
        "link": "#七、参考资料",
        "children": []
      }
    ],
    "path": "/qa/q28-kbengine-registry-actor-transport.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "Q29: KBEngine CellApp 之间如何通信？如何发现自己的空间位置？",
    "headers": [
      {
        "level": 2,
        "title": "问题分析",
        "slug": "问题分析",
        "link": "#问题分析",
        "children": []
      },
      {
        "level": 2,
        "title": "一、CellApp 通信架构",
        "slug": "一、cellapp-通信架构",
        "link": "#一、cellapp-通信架构",
        "children": [
          {
            "level": 3,
            "title": "通信拓扑",
            "slug": "通信拓扑",
            "link": "#通信拓扑",
            "children": []
          },
          {
            "level": 3,
            "title": "通信方式",
            "slug": "通信方式",
            "link": "#通信方式",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "二、CellApp 启动与空间分配流程",
        "slug": "二、cellapp-启动与空间分配流程",
        "link": "#二、cellapp-启动与空间分配流程",
        "children": [
          {
            "level": 3,
            "title": "启动流程",
            "slug": "启动流程",
            "link": "#启动流程",
            "children": []
          },
          {
            "level": 3,
            "title": "空间分配策略",
            "slug": "空间分配策略",
            "link": "#空间分配策略",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "三、空间位置发现机制",
        "slug": "三、空间位置发现机制",
        "link": "#三、空间位置发现机制",
        "children": [
          {
            "level": 3,
            "title": "CellApp 的空间元数据",
            "slug": "cellapp-的空间元数据",
            "link": "#cellapp-的空间元数据",
            "children": []
          },
          {
            "level": 3,
            "title": "边界感知",
            "slug": "边界感知",
            "link": "#边界感知",
            "children": []
          },
          {
            "level": 3,
            "title": "邻居方向定义（3D）",
            "slug": "邻居方向定义-3d",
            "link": "#邻居方向定义-3d",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "四、CellApp 间通信协议",
        "slug": "四、cellapp-间通信协议",
        "link": "#四、cellapp-间通信协议",
        "children": [
          {
            "level": 3,
            "title": "通信类型",
            "slug": "通信类型",
            "link": "#通信类型",
            "children": []
          },
          {
            "level": 3,
            "title": "Entity 迁移协议",
            "slug": "entity-迁移协议",
            "link": "#entity-迁移协议",
            "children": []
          },
          {
            "level": 3,
            "title": "Ghost 同步协议",
            "slug": "ghost-同步协议",
            "link": "#ghost-同步协议",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "五、边界协调机制",
        "slug": "五、边界协调机制",
        "link": "#五、边界协调机制",
        "children": [
          {
            "level": 3,
            "title": "边界宽度设计",
            "slug": "边界宽度设计",
            "link": "#边界宽度设计",
            "children": []
          },
          {
            "level": 3,
            "title": "跨边界判断",
            "slug": "跨边界判断",
            "link": "#跨边界判断",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "六、邻居发现与维护",
        "slug": "六、邻居发现与维护",
        "link": "#六、邻居发现与维护",
        "children": [
          {
            "level": 3,
            "title": "邻居表维护",
            "slug": "邻居表维护",
            "link": "#邻居表维护",
            "children": []
          },
          {
            "level": 3,
            "title": "邻居变化通知",
            "slug": "邻居变化通知",
            "link": "#邻居变化通知",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "七、动态空间调整",
        "slug": "七、动态空间调整",
        "link": "#七、动态空间调整",
        "children": [
          {
            "level": 3,
            "title": "负载触发的边界调整",
            "slug": "负载触发的边界调整",
            "link": "#负载触发的边界调整",
            "children": []
          },
          {
            "level": 3,
            "title": "边界调整协议",
            "slug": "边界调整协议",
            "link": "#边界调整协议",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "八、通信优化",
        "slug": "八、通信优化",
        "link": "#八、通信优化",
        "children": [
          {
            "level": 3,
            "title": "批量 Ghost 更新",
            "slug": "批量-ghost-更新",
            "link": "#批量-ghost-更新",
            "children": []
          },
          {
            "level": 3,
            "title": "消息优先级",
            "slug": "消息优先级",
            "link": "#消息优先级",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "九、故障处理",
        "slug": "九、故障处理",
        "link": "#九、故障处理",
        "children": [
          {
            "level": 3,
            "title": "CellApp 故障检测",
            "slug": "cellapp-故障检测",
            "link": "#cellapp-故障检测",
            "children": []
          },
          {
            "level": 3,
            "title": "故障恢复策略",
            "slug": "故障恢复策略",
            "link": "#故障恢复策略",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "十、参考资料",
        "slug": "十、参考资料",
        "link": "#十、参考资料",
        "children": []
      }
    ],
    "path": "/qa/q29-cellapp-communication-space-discovery.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "Q3: 什么是 AOI（Area of Interest）？有哪些实现方式？各有什么优缺点？",
    "headers": [
      {
        "level": 2,
        "title": "问题分析",
        "slug": "问题分析",
        "link": "#问题分析",
        "children": []
      },
      {
        "level": 2,
        "title": "AOI 基本概念",
        "slug": "aoi-基本概念",
        "link": "#aoi-基本概念",
        "children": [
          {
            "level": 3,
            "title": "什么是 AOI？",
            "slug": "什么是-aoi",
            "link": "#什么是-aoi",
            "children": []
          },
          {
            "level": 3,
            "title": "AOI 的作用",
            "slug": "aoi-的作用",
            "link": "#aoi-的作用",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "分层 AOI 设计",
        "slug": "分层-aoi-设计",
        "link": "#分层-aoi-设计",
        "children": [
          {
            "level": 3,
            "title": "为什么需要分层？",
            "slug": "为什么需要分层",
            "link": "#为什么需要分层",
            "children": []
          },
          {
            "level": 3,
            "title": "分层 AOI 架构",
            "slug": "分层-aoi-架构",
            "link": "#分层-aoi-架构",
            "children": []
          },
          {
            "level": 3,
            "title": "分层 AOI 实现",
            "slug": "分层-aoi-实现",
            "link": "#分层-aoi-实现",
            "children": []
          },
          {
            "level": 3,
            "title": "分层 AOI 的优势",
            "slug": "分层-aoi-的优势",
            "link": "#分层-aoi-的优势",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "AOI 实现方式对比",
        "slug": "aoi-实现方式对比",
        "link": "#aoi-实现方式对比",
        "children": [
          {
            "level": 3,
            "title": "1. 九宫格算法",
            "slug": "_1-九宫格算法",
            "link": "#_1-九宫格算法",
            "children": []
          },
          {
            "level": 3,
            "title": "2. 十字链表",
            "slug": "_2-十字链表",
            "link": "#_2-十字链表",
            "children": []
          },
          {
            "level": 3,
            "title": "3. 空间哈希",
            "slug": "_3-空间哈希",
            "link": "#_3-空间哈希",
            "children": []
          },
          {
            "level": 3,
            "title": "对比总结",
            "slug": "对比总结",
            "link": "#对比总结",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "CellApp 中的 AOI",
        "slug": "cellapp-中的-aoi",
        "link": "#cellapp-中的-aoi",
        "children": [
          {
            "level": 3,
            "title": "CellApp 中的 AOI 职责",
            "slug": "cellapp-中的-aoi-职责",
            "link": "#cellapp-中的-aoi-职责",
            "children": []
          },
          {
            "level": 3,
            "title": "CellApp AOI 实现",
            "slug": "cellapp-aoi-实现",
            "link": "#cellapp-aoi-实现",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "CellApp 重叠部分的处理",
        "slug": "cellapp-重叠部分的处理",
        "link": "#cellapp-重叠部分的处理",
        "children": [
          {
            "level": 3,
            "title": "问题：边界处的 Entity",
            "slug": "问题-边界处的-entity",
            "link": "#问题-边界处的-entity",
            "children": []
          },
          {
            "level": 3,
            "title": "解决方案：Ghost 机制",
            "slug": "解决方案-ghost-机制",
            "link": "#解决方案-ghost-机制",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "性能优化",
        "slug": "性能优化",
        "link": "#性能优化",
        "children": [
          {
            "level": 3,
            "title": "1. 分帧处理",
            "slug": "_1-分帧处理",
            "link": "#_1-分帧处理",
            "children": []
          },
          {
            "level": 3,
            "title": "2. 空间索引",
            "slug": "_2-空间索引",
            "link": "#_2-空间索引",
            "children": []
          },
          {
            "level": 3,
            "title": "3. 事件合并",
            "slug": "_3-事件合并",
            "link": "#_3-事件合并",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "参考资料",
        "slug": "参考资料",
        "link": "#参考资料",
        "children": []
      }
    ],
    "path": "/qa/q3-aoi-implementation.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "Q30: KBEngine 的 Space 是什么？与物理空间划分有什么区别？",
    "headers": [
      {
        "level": 2,
        "title": "问题分析",
        "slug": "问题分析",
        "link": "#问题分析",
        "children": []
      },
      {
        "level": 2,
        "title": "一、Space 的真实定义",
        "slug": "一、space-的真实定义",
        "link": "#一、space-的真实定义",
        "children": [
          {
            "level": 3,
            "title": "官方定义",
            "slug": "官方定义",
            "link": "#官方定义",
            "children": []
          },
          {
            "level": 3,
            "title": "关键特性",
            "slug": "关键特性",
            "link": "#关键特性",
            "children": []
          },
          {
            "level": 3,
            "title": "源码证据",
            "slug": "源码证据",
            "link": "#源码证据",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "二、Space vs 物理空间划分",
        "slug": "二、space-vs-物理空间划分",
        "link": "#二、space-vs-物理空间划分",
        "children": [
          {
            "level": 3,
            "title": "常见误解",
            "slug": "常见误解",
            "link": "#常见误解",
            "children": []
          },
          {
            "level": 3,
            "title": "KBEngine 的实际设计",
            "slug": "kbengine-的实际设计",
            "link": "#kbengine-的实际设计",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "三、Space 与 CoordinateSystem 的区别",
        "slug": "三、space-与-coordinatesystem-的区别",
        "link": "#三、space-与-coordinatesystem-的区别",
        "children": [
          {
            "level": 3,
            "title": "对比表",
            "slug": "对比表",
            "link": "#对比表",
            "children": []
          },
          {
            "level": 3,
            "title": "CoordinateSystem 的实际作用",
            "slug": "coordinatesystem-的实际作用",
            "link": "#coordinatesystem-的实际作用",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "四、Space 的创建和管理",
        "slug": "四、space-的创建和管理",
        "link": "#四、space-的创建和管理",
        "children": [
          {
            "level": 3,
            "title": "Space 创建流程",
            "slug": "space-创建流程",
            "link": "#space-创建流程",
            "children": []
          },
          {
            "level": 3,
            "title": "Space 负载均衡",
            "slug": "space-负载均衡",
            "link": "#space-负载均衡",
            "children": []
          },
          {
            "level": 3,
            "title": "指定 Space 到特定 CellApp",
            "slug": "指定-space-到特定-cellapp",
            "link": "#指定-space-到特定-cellapp",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "五、Space 的边界管理",
        "slug": "五、space-的边界管理",
        "link": "#五、space-的边界管理",
        "children": [
          {
            "level": 3,
            "title": "Space 边界 vs 物理边界",
            "slug": "space-边界-vs-物理边界",
            "link": "#space-边界-vs-物理边界",
            "children": []
          },
          {
            "level": 3,
            "title": "边界的用途",
            "slug": "边界的用途",
            "link": "#边界的用途",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "六、多 Space 管理",
        "slug": "六、多-space-管理",
        "link": "#六、多-space-管理",
        "children": [
          {
            "level": 3,
            "title": "一个 CellApp 多 Space 示例",
            "slug": "一个-cellapp-多-space-示例",
            "link": "#一个-cellapp-多-space-示例",
            "children": []
          },
          {
            "level": 3,
            "title": "Space 之间 Entity 迁移",
            "slug": "space-之间-entity-迁移",
            "link": "#space-之间-entity-迁移",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "七、与物理空间划分的对比",
        "slug": "七、与物理空间划分的对比",
        "link": "#七、与物理空间划分的对比",
        "children": [
          {
            "level": 3,
            "title": "不同引擎的 Space 概念",
            "slug": "不同引擎的-space-概念",
            "link": "#不同引擎的-space-概念",
            "children": []
          },
          {
            "level": 3,
            "title": "KBEngine 的选择",
            "slug": "kbengine-的选择",
            "link": "#kbengine-的选择",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "八、实际应用示例",
        "slug": "八、实际应用示例",
        "link": "#八、实际应用示例",
        "children": [
          {
            "level": 3,
            "title": "场景：MMO 的多 Space 管理",
            "slug": "场景-mmo-的多-space-管理",
            "link": "#场景-mmo-的多-space-管理",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "九、常见问题",
        "slug": "九、常见问题",
        "link": "#九、常见问题",
        "children": [
          {
            "level": 3,
            "title": "Q1: Entity 如何跨 Space 移动？",
            "slug": "q1-entity-如何跨-space-移动",
            "link": "#q1-entity-如何跨-space-移动",
            "children": []
          },
          {
            "level": 3,
            "title": "Q2: 不同 Space 的 Entity 能交互吗？",
            "slug": "q2-不同-space-的-entity-能交互吗",
            "link": "#q2-不同-space-的-entity-能交互吗",
            "children": []
          },
          {
            "level": 3,
            "title": "Q3: CoordinateSystem 是全局的还是每个 Space 独立？",
            "slug": "q3-coordinatesystem-是全局的还是每个-space-独立",
            "link": "#q3-coordinatesystem-是全局的还是每个-space-独立",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "十、参考资料",
        "slug": "十、参考资料",
        "link": "#十、参考资料",
        "children": []
      }
    ],
    "path": "/qa/q30-kbengine-space-concept.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "Q31: KBEngine 如何高效广播？如何保证消息不重复？",
    "headers": [
      {
        "level": 2,
        "title": "问题分析",
        "slug": "问题分析",
        "link": "#问题分析",
        "children": []
      },
      {
        "level": 2,
        "title": "一、KBEngine 的 AOI 实现",
        "slug": "一、kbengine-的-aoi-实现",
        "link": "#一、kbengine-的-aoi-实现",
        "children": [
          {
            "level": 3,
            "title": "三轴十字链表",
            "slug": "三轴十字链表",
            "link": "#三轴十字链表",
            "children": []
          },
          {
            "level": 3,
            "title": "CoordinateSystem 核心",
            "slug": "coordinatesystem-核心",
            "link": "#coordinatesystem-核心",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "二、ViewEntity 和 Witness 机制",
        "slug": "二、viewentity-和-witness-机制",
        "link": "#二、viewentity-和-witness-机制",
        "children": [
          {
            "level": 3,
            "title": "视野管理架构",
            "slug": "视野管理架构",
            "link": "#视野管理架构",
            "children": []
          },
          {
            "level": 3,
            "title": "Witness 生命周期",
            "slug": "witness-生命周期",
            "link": "#witness-生命周期",
            "children": []
          },
          {
            "level": 3,
            "title": "onEnterWitness / onLeaveWitness",
            "slug": "onenterwitness-onleavewitness",
            "link": "#onenterwitness-onleavewitness",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "三、广播去重机制",
        "slug": "三、广播去重机制",
        "link": "#三、广播去重机制",
        "children": [
          {
            "level": 3,
            "title": "去重策略 1：ViewEntity 自动去重",
            "slug": "去重策略-1-viewentity-自动去重",
            "link": "#去重策略-1-viewentity-自动去重",
            "children": []
          },
          {
            "level": 3,
            "title": "去重策略 2：坐标系层级",
            "slug": "去重策略-2-坐标系层级",
            "link": "#去重策略-2-坐标系层级",
            "children": []
          },
          {
            "level": 3,
            "title": "去重策略 3：帧标记",
            "slug": "去重策略-3-帧标记",
            "link": "#去重策略-3-帧标记",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "四、高效广播策略",
        "slug": "四、高效广播策略",
        "link": "#四、高效广播策略",
        "children": [
          {
            "level": 3,
            "title": "策略 1：只广播视野内的变化",
            "slug": "策略-1-只广播视野内的变化",
            "link": "#策略-1-只广播视野内的变化",
            "children": []
          },
          {
            "level": 3,
            "title": "策略 2：增量广播",
            "slug": "策略-2-增量广播",
            "link": "#策略-2-增量广播",
            "children": []
          },
          {
            "level": 3,
            "title": "策略 3：频率控制",
            "slug": "策略-3-频率控制",
            "link": "#策略-3-频率控制",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "五、完整的广播流程",
        "slug": "五、完整的广播流程",
        "link": "#五、完整的广播流程",
        "children": [
          {
            "level": 3,
            "title": "Entity 移动时的广播",
            "slug": "entity-移动时的广播",
            "link": "#entity-移动时的广播",
            "children": []
          },
          {
            "level": 3,
            "title": "多人广播去重图解",
            "slug": "多人广播去重图解",
            "link": "#多人广播去重图解",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "六、性能优化技巧",
        "slug": "六、性能优化技巧",
        "link": "#六、性能优化技巧",
        "children": [
          {
            "level": 3,
            "title": "1. 视野半径控制",
            "slug": "_1-视野半径控制",
            "link": "#_1-视野半径控制",
            "children": []
          },
          {
            "level": 3,
            "title": "2. 分层广播",
            "slug": "_2-分层广播",
            "link": "#_2-分层广播",
            "children": []
          },
          {
            "level": 3,
            "title": "3. 批量打包",
            "slug": "_3-批量打包",
            "link": "#_3-批量打包",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "七、常见问题",
        "slug": "七、常见问题",
        "link": "#七、常见问题",
        "children": [
          {
            "level": 3,
            "title": "Q1: 如果两个 Entity 同时移动，会重复广播吗？",
            "slug": "q1-如果两个-entity-同时移动-会重复广播吗",
            "link": "#q1-如果两个-entity-同时移动-会重复广播吗",
            "children": []
          },
          {
            "level": 3,
            "title": "Q2: 客户端如何知道哪些 Entity 在视野内？",
            "slug": "q2-客户端如何知道哪些-entity-在视野内",
            "link": "#q2-客户端如何知道哪些-entity-在视野内",
            "children": []
          },
          {
            "level": 3,
            "title": "Q3: 如何避免视野边界频繁进出？",
            "slug": "q3-如何避免视野边界频繁进出",
            "link": "#q3-如何避免视野边界频繁进出",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "八、参考资料",
        "slug": "八、参考资料",
        "link": "#八、参考资料",
        "children": []
      }
    ],
    "path": "/qa/q31-kbengine-broadcast-dedup.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "Q9: CellAppMgr 如果宕机了怎么办？有哪些解决方案？",
    "headers": [
      {
        "level": 2,
        "title": "问题分析",
        "slug": "问题分析",
        "link": "#问题分析",
        "children": []
      },
      {
        "level": 2,
        "title": "KBEngine 的实际情况",
        "slug": "kbengine-的实际情况",
        "link": "#kbengine-的实际情况",
        "children": [
          {
            "level": 3,
            "title": "官方确认",
            "slug": "官方确认",
            "link": "#官方确认",
            "children": []
          },
          {
            "level": 3,
            "title": "宕机影响",
            "slug": "宕机影响",
            "link": "#宕机影响",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "解决方案",
        "slug": "解决方案",
        "link": "#解决方案",
        "children": [
          {
            "level": 3,
            "title": "方案 1: 进程监控 + 自动重启（最简单）",
            "slug": "方案-1-进程监控-自动重启-最简单",
            "link": "#方案-1-进程监控-自动重启-最简单",
            "children": []
          },
          {
            "level": 3,
            "title": "方案 2: 状态持久化 + 快速恢复",
            "slug": "方案-2-状态持久化-快速恢复",
            "link": "#方案-2-状态持久化-快速恢复",
            "children": []
          },
          {
            "level": 3,
            "title": "方案 3: 主备模式（需修改源码）",
            "slug": "方案-3-主备模式-需修改源码",
            "link": "#方案-3-主备模式-需修改源码",
            "children": []
          },
          {
            "level": 3,
            "title": "方案 4: RAFT + ETCD（推荐）",
            "slug": "方案-4-raft-etcd-推荐",
            "link": "#方案-4-raft-etcd-推荐",
            "children": []
          }
        ]
      },
      {
        "level": 2,
        "title": "方案对比",
        "slug": "方案对比",
        "link": "#方案对比",
        "children": []
      },
      {
        "level": 2,
        "title": "Apollo 建议方案",
        "slug": "apollo-建议方案",
        "link": "#apollo-建议方案",
        "children": []
      },
      {
        "level": 2,
        "title": "参考资料",
        "slug": "参考资料",
        "link": "#参考资料",
        "children": []
      }
    ],
    "path": "/qa/q9-cellappmgr-failure.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "Unity 属性同步 SDK",
    "headers": [
      {
        "level": 2,
        "title": "概述",
        "slug": "概述",
        "link": "#概述",
        "children": []
      },
      {
        "level": 2,
        "title": "目录结构",
        "slug": "目录结构",
        "link": "#目录结构",
        "children": []
      },
      {
        "level": 2,
        "title": "服务端对应关系",
        "slug": "服务端对应关系",
        "link": "#服务端对应关系",
        "children": []
      },
      {
        "level": 2,
        "title": "属性 ID 范围",
        "slug": "属性-id-范围",
        "link": "#属性-id-范围",
        "children": []
      },
      {
        "level": 2,
        "title": "使用示例",
        "slug": "使用示例",
        "link": "#使用示例",
        "children": []
      },
      {
        "level": 2,
        "title": "开发任务",
        "slug": "开发任务",
        "link": "#开发任务",
        "children": [
          {
            "level": 3,
            "title": "阶段一：基础类型",
            "slug": "阶段一-基础类型",
            "link": "#阶段一-基础类型",
            "children": []
          },
          {
            "level": 3,
            "title": "阶段二：注册与容器",
            "slug": "阶段二-注册与容器",
            "link": "#阶段二-注册与容器",
            "children": []
          },
          {
            "level": 3,
            "title": "阶段三：同步系统",
            "slug": "阶段三-同步系统",
            "link": "#阶段三-同步系统",
            "children": []
          },
          {
            "level": 3,
            "title": "阶段四：计算属性",
            "slug": "阶段四-计算属性",
            "link": "#阶段四-计算属性",
            "children": []
          },
          {
            "level": 3,
            "title": "阶段五：事件系统",
            "slug": "阶段五-事件系统",
            "link": "#阶段五-事件系统",
            "children": []
          },
          {
            "level": 3,
            "title": "阶段六：扩展与工具",
            "slug": "阶段六-扩展与工具",
            "link": "#阶段六-扩展与工具",
            "children": []
          }
        ]
      }
    ],
    "path": "/sdks/unity/Attribute_SDK.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "Apollo SDK 目录结构",
    "headers": [
      {
        "level": 2,
        "title": "概述",
        "slug": "概述",
        "link": "#概述",
        "children": []
      },
      {
        "level": 2,
        "title": "目录结构",
        "slug": "目录结构",
        "link": "#目录结构",
        "children": []
      },
      {
        "level": 2,
        "title": "Unity SDK 模块说明",
        "slug": "unity-sdk-模块说明",
        "link": "#unity-sdk-模块说明",
        "children": []
      },
      {
        "level": 2,
        "title": "文档规范",
        "slug": "文档规范",
        "link": "#文档规范",
        "children": []
      }
    ],
    "path": "/sdks/unity/SDK_Structure.html",
    "pathLocale": "/",
    "extraFields": []
  },
  {
    "title": "",
    "headers": [],
    "path": "/404.html",
    "pathLocale": "/",
    "extraFields": []
  }
]

if (import.meta.webpackHot) {
  import.meta.webpackHot.accept()
  if (__VUE_HMR_RUNTIME__.updateSearchIndex) {
    __VUE_HMR_RUNTIME__.updateSearchIndex(searchIndex)
  }
}

if (import.meta.hot) {
  import.meta.hot.accept(({ searchIndex }) => {
    __VUE_HMR_RUNTIME__.updateSearchIndex(searchIndex)
  })
}
