# Spring Bean 生命周期与 Scope 知识点总结

## 目录
1. [Bean 的 Scope 类型](#1-bean-的-scope-类型)
2. [Bean 的完整生命周期](#2-bean-的完整生命周期)
3. [生命周期回调方法](#3-生命周期回调方法)
4. [Bean 的作用域与生命周期关系](#4-bean-的作用域与生命周期关系)
5. [Aware 接口](#5-aware-接口)
6. [Bean 后置处理器](#6-bean-后置处理器)
7. [最佳实践](#7-最佳实践)
8. [常见问题与解决方案](#8-常见问题与解决方案)

---

## 1. Bean 的 Scope 类型

### 1.1 六种内置 Scope

| Scope 类型 | 说明 | 创建时机 | 销毁时机 | 使用场景 | 是否线程安全 |
|-----------|------|----------|----------|----------|-------------|
| **singleton** | 默认 Scope，整个容器只有一个实例 | 容器启动时（非延迟加载） | 容器关闭时 | 无状态服务类、配置类、工具类 | 需要 |
| **prototype** | 每次请求都创建新实例 | 每次获取时 | GC 回收时 | 有状态对象、命令对象、DTO | 不需要 |
| **request** | 每个 HTTP 请求一个实例 | 每个 HTTP 请求开始时 | HTTP 请求结束时 | 请求数据、表单对象 | 需要 |
| **session** | 每个 HTTP Session 一个实例 | Session 创建时 | Session 销毁时 | 用户会话数据、购物车 | 需要 |
| **application** | 每个 ServletContext 一个实例 | Web 应用启动时 | Web 应用关闭时 | 全局配置、应用级缓存 | 需要 |
| **websocket** | 每个 WebSocket 一个实例 | WebSocket 连接建立时 | WebSocket 连接关闭时 | WebSocket 会话数据 | 需要 |

### 1.2 Scope 定义方式

```java
// 1. 使用 @Scope 注解
@Component
@Scope("prototype")  // 每次注入都创建新实例
public class RequestHandler {
    // ...
}

// 2. 使用 XML 配置
<bean id="requestHandler" class="com.example.RequestHandler" scope="prototype"/>

// 3. 使用 @Scope 的 proxyMode（解决单例注入原型的问题）
@Component
@Scope(value = "prototype", proxyMode = ScopedProxyMode.TARGET_CLASS)
public class PrototypeBean {
    // ...
}
```

### 1.3 自定义 Scope

```java
// 1. 实现 Scope 接口
public class ThreadScope implements Scope {
    private final ThreadLocal<Map<String, Object>> threadScope =
        ThreadLocal.withInitial(HashMap::new);

    @Override
    public Object get(String name, ObjectFactory<?> objectFactory) {
        return threadScope.get().computeIfAbsent(name, k -> objectFactory.getObject());
    }

    @Override
    public Object remove(String name) {
        return threadScope.get().remove(name);
    }

    @Override
    public void registerDestructionCallback(String name, Runnable callback) {
        // 注册销毁回调
    }

    // 其他方法实现...
}

// 2. 注册自定义 Scope
@Component
public class CustomScopeConfigurer implements BeanFactoryPostProcessor {
    @Override
    public void postProcessBeanFactory(ConfigurableListableBeanFactory factory) {
        factory.registerScope("thread", new ThreadScope());
    }
}

// 3. 使用自定义 Scope
@Component
@Scope("thread")
public class ThreadLocalBean {
    // 每个线程一个实例
}
```

---

## 2. Bean 的完整生命周期

### 2.1 生命周期流程图

```
1. 实例化（Instantiation）
   ↓
2. 属性赋值（Populate Properties）
   ↓
3. BeanNameAware 的 setBeanName()
   ↓
4. BeanFactoryAware 的 setBeanFactory()
   ↓
5. ApplicationContextAware 的 setApplicationContext()
   ↓
6. BeanPostProcessor 的 postProcessBeforeInitialization()
   ↓
7. @PostConstruct 注解的方法
   ↓
8. InitializingBean 的 afterPropertiesSet()
   ↓
9. 自定义 init-method
   ↓
10. BeanPostProcessor 的 postProcessAfterInitialization()
    ↓
11. Bean 可以使用
    ↓
12. @PreDestroy 注解的方法
    ↓
13. DisposableBean 的 destroy()
    ↓
14. 自定义 destroy-method
```

### 2.2 生命周期详解

#### 阶段一：创建阶段

```java
// 1. 实例化
// Spring 通过反射调用构造方法创建 Bean 实例
// 此时 Bean 只是一个空对象，属性还未设置

// 2. 属性赋值
// Spring 根据配置填充 Bean 的属性
// 包括：@Value 注入、@Autowired 注入、XML 配置的属性等

// 3. Aware 接口回调
// 如果 Bean 实现了特定的 Aware 接口，Spring 会调用相应方法
```

#### 阶段二：初始化阶段

```java
// 4. 初始化前处理
// 所有 BeanPostProcessor 的 postProcessBeforeInitialization 方法被调用

// 5. 初始化方法执行（按顺序）
// - @PostConstruct 标注的方法
// - InitializingBean 接口的 afterPropertiesSet() 方法
// - 自定义的 init-method 方法

// 6. 初始化后处理
// 所有 BeanPostProcessor 的 postProcessAfterInitialization 方法被调用
```

#### 阶段三：使用阶段

```java
// 7. Bean 准备就绪
// 此时 Bean 已经完全初始化，可以被其他 Bean 使用
// 可以被注入到其他 Bean 中，或通过 getBean() 获取
```

#### 阶段四：销毁阶段

```java
// 8. 销毁前处理
// 容器关闭时，触发销毁流程

// 9. 销毁方法执行（按顺序）
// - @PreDestroy 标注的方法
// - DisposableBean 接口的 destroy() 方法
// - 自定义的 destroy-method 方法
```

---

## 3. 生命周期回调方法

### 3.1 JSR-250 注解（推荐方式）

```java
@Component
public class LifecycleBean {

    @PostConstruct  // 初始化后调用
    public void init() {
        System.out.println("Bean 初始化完成");
        // 执行初始化逻辑：验证配置、开启资源等
    }

    @PreDestroy  // 销毁前调用
    public void cleanup() {
        System.out.println("Bean 即将销毁");
        // 执行清理逻辑：关闭连接、释放资源等
    }
}
```

### 3.2 实现 InitializingBean 和 DisposableBean

```java
@Component
public class LifecycleBean implements InitializingBean, DisposableBean {

    @Override
    public void afterPropertiesSet() {
        System.out.println("通过 InitializingBean 初始化");
        // 不推荐，与 Spring 框架耦合
    }

    @Override
    public void destroy() {
        System.out.println("通过 DisposableBean 销毁");
        // 不推荐，与 Spring 框架耦合
    }
}
```

### 3.3 自定义 init-method 和 destroy-method

```java
// 方式一：XML 配置
<bean id="lifecycleBean"
      class="com.example.LifecycleBean"
      init-method="customInit"
      destroy-method="customDestroy"/>

// 方式二：@Bean 注解
@Configuration
public class AppConfig {
    @Bean(initMethod = "customInit", destroyMethod = "customDestroy")
    public LifecycleBean lifecycleBean() {
        return new LifecycleBean();
    }
}

// 方式三：全局配置（推荐）
@Configuration
public class AppConfig {

    @Bean
    public static BeanPostProcessor beanPostProcessor() {
        return new CommonAnnotationBeanPostProcessor();
    }
}

// Bean 类
public class LifecycleBean {

    public void customInit() {
        System.out.println("自定义初始化方法");
    }

    public void customDestroy() {
        System.out.println("自定义销毁方法");
    }
}
```

### 3.4 多种初始化方式的执行顺序

```java
@Component
public class BeanWithMultipleCallbacks
    implements InitializingBean, DisposableBean {

    @PostConstruct
    public void postConstruct() {
        // 1. 第一个执行
        System.out.println("@PostConstruct");
    }

    @Override
    public void afterPropertiesSet() {
        // 2. 第二个执行
        System.out.println("afterPropertiesSet");
    }

    public void initMethod() {
        // 3. 第三个执行
        System.out.println("init-method");
    }

    // 使用时
    @PreDestroy
    public void preDestroy() {
        // 1. 销毁时第一个执行
        System.out.println("@PreDestroy");
    }

    @Override
    public void destroy() {
        // 2. 销毁时第二个执行
        System.out.println("destroy");
    }

    public void destroyMethod() {
        // 3. 销毁时第三个执行
        System.out.println("destroy-method");
    }
}
```

---

## 4. Bean 的作用域与生命周期关系

### 4.1 Singleton Bean 的生命周期

```java
@Component
@Scope("singleton")  // 默认值
public class SingletonBean {

    @PostConstruct
    public void init() {
        // 在容器启动时调用一次
        System.out.println("SingletonBean 初始化");
    }

    @PreDestroy
    public void destroy() {
        // 在容器关闭时调用一次
        System.out.println("SingletonBean 销毁");
    }
}

// 生命周期：
// 1. 容器启动 → 创建实例（如果非延迟加载）
// 2. 调用 init 方法
// 3. 在整个容器生命周期内保持活动
// 4. 容器关闭 → 调用 destroy 方法 → 销毁实例
```

### 4.2 Prototype Bean 的生命周期

```java
@Component
@Scope("prototype")
public class PrototypeBean {

    @PostConstruct
    public void init() {
        // 每次获取 Bean 时都调用
        System.out.println("PrototypeBean 初始化");
    }

    @PreDestroy
    public void destroy() {
        // 注意：Spring 不会自动调用！
        System.out.println("PrototypeBean 销毁");
    }
}

// 生命周期：
// 1. 每次调用 getBean() 或注入时创建新实例
// 2. 调用 init 方法
// 3. Spring 不再管理此实例的生命周期
// 4. 需要自己负责销毁（调用 destroy() 或等待 GC）
```

### 4.3 Request Bean 的生命周期（Web 环境）

```java
@Component
@Scope(value = WebApplicationContext.SCOPE_REQUEST,
       proxyMode = ScopedProxyMode.TARGET_CLASS)
public class RequestBean {

    @PostConstruct
    public void init() {
        // 每个 HTTP 请求开始时调用
        System.out.println("RequestBean 初始化");
    }

    @PreDestroy
    public void destroy() {
        // 每个 HTTP 请求结束时调用
        System.out.println("RequestBean 销毁");
    }
}
```

### 4.4 Session Bean 的生命周期（Web 环境）

```java
@Component
@Scope(value = WebApplicationContext.SCOPE_SESSION,
       proxyMode = ScopedProxyMode.TARGET_CLASS)
public class SessionBean {

    @PostConstruct
    public void init() {
        // Session 创建时调用
        System.out.println("SessionBean 初始化");
    }

    @PreDestroy
    public void destroy() {
        // Session 销毁时调用
        System.out.println("SessionBean 销毁");
    }
}
```

---

## 5. Aware 接口

### 5.1 常用 Aware 接口列表

| 接口 | 方法 | 说明 | 使用场景 |
|------|------|------|----------|
| BeanNameAware | setBeanName(String) | 获取 Bean 的名称 | 日志记录、配置查找 |
| BeanFactoryAware | setBeanFactory(BeanFactory) | 获取 BeanFactory | 动态获取其他 Bean |
| ApplicationContextAware | setApplicationContext(ApplicationContext) | 获取应用上下文 | 获取环境信息、事件发布 |
| MessageSourceAware | setMessageSource(MessageSource) | 获取国际化资源 | 多语言支持 |
| ResourceLoaderAware | setResourceLoader(ResourceLoader) | 获取资源加载器 | 加载配置文件 |
| ApplicationEventPublisherAware | setApplicationEventPublisher(ApplicationEventPublisher) | 获取事件发布器 | 发布应用事件 |

### 5.2 Aware 接口使用示例

```java
@Component
public class AwareExampleBean implements
    BeanNameAware,
    BeanFactoryAware,
    ApplicationContextAware {

    private String beanName;
    private BeanFactory beanFactory;
    private ApplicationContext applicationContext;

    @Override
    public void setBeanName(String name) {
        this.beanName = name;
        System.out.println("Bean 名称: " + name);
    }

    @Override
    public void setBeanFactory(BeanFactory beanFactory) {
        this.beanFactory = beanFactory;
        System.out.println("获取到 BeanFactory");

        // 动态获取其他 Bean
        OtherBean otherBean = beanFactory.getBean("otherBean", OtherBean.class);
    }

    @Override
    public void setApplicationContext(ApplicationContext applicationContext) {
        this.applicationContext = applicationContext;
        System.out.println("获取到 ApplicationContext");

        // 发布事件
        applicationContext.publishEvent(new MyApplicationEvent("事件内容"));

        // 获取环境信息
        String activeProfile = applicationContext.getEnvironment().getActiveProfiles()[0];
        System.out.println("当前激活的 Profile: " + activeProfile);
    }

    // 使用获取到的信息
    public void doSomething() {
        System.out.println("Bean " + beanName + " 正在执行...");

        // 获取配置信息
        String value = applicationContext.getEnvironment().getProperty("app.name");
        System.out.println("应用名称: " + value);
    }
}
```

---

## 6. Bean 后置处理器

### 6.1 BeanPostProcessor 接口

```java
@Component
public class CustomBeanPostProcessor implements BeanPostProcessor {

    @Override
    public Object postProcessBeforeInitialization(Object bean, String beanName) {
        // 在初始化方法之前调用
        // 返回值可以是原 bean，也可以是包装后的 bean

        // 示例：为所有 Service 添加日志代理
        if (beanName.endsWith("Service")) {
            System.out.println("为 Service " + beanName + " 创建日志代理");
            // return createLoggingProxy(bean);
        }

        return bean;
    }

    @Override
    public Object postProcessAfterInitialization(Object bean, String beanName) {
        // 在初始化方法之后调用
        // 此时 Bean 已经完全初始化

        // 示例：验证必填属性
        if (bean instanceof Validatable) {
            Validatable validatable = (Validatable) bean;
            if (!validatable.validate()) {
                throw new BeanCreationException("Bean " + beanName + " 验证失败");
            }
        }

        return bean;
    }
}
```

### 6.2 常用的内置 BeanPostProcessor

```java
// 1. AutowiredAnnotationBeanPostProcessor
// 处理 @Autowired、@Value、@Inject 注解

// 2. CommonAnnotationBeanPostProcessor
// 处理 @PostConstruct、@PreDestroy、@Resource 注解

// 3. RequiredAnnotationBeanPostProcessor
// 处理 @Required 注解（已废弃）

// 4. ConfigurationClassPostProcessor
// 处理 @Configuration 注解

// 5. AsyncAnnotationBeanPostProcessor
// 处理 @Async 注解

// 6. ScheduledAnnotationBeanPostProcessor
// 处理 @Scheduled 注解

// 7. AbstractAutoProxyCreator
// 创建 AOP 代理
```

### 6.3 自定义 BeanPostProcessor 实现特定功能

```java
@Component
public class PerformanceMonitorBeanPostProcessor implements BeanPostProcessor {

    private Map<String, Long> startTimes = new ConcurrentHashMap<>();

    @Override
    public Object postProcessBeforeInitialization(Object bean, String beanName) {
        startTimes.put(beanName, System.currentTimeMillis());
        return bean;
    }

    @Override
    public Object postProcessAfterInitialization(Object bean, String beanName) {
        Long startTime = startTimes.remove(beanName);
        if (startTime != null) {
            long duration = System.currentTimeMillis() - startTime;
            if (duration > 100) {  // 超过 100ms 记录
                System.out.println("警告: Bean " + beanName + " 初始化耗时 " + duration + "ms");
            }
        }
        return bean;
    }
}

// AOP 相关的后置处理器
@Component
public class CustomAopProxyCreator extends AbstractAutoProxyCreator {

    @Override
    protected Object[] getAdvicesAndAdvisorsForBean(
            Class<?> beanClass, String beanName, TargetSource targetSource) {

        // 为所有带 @Monitor 注解的方法创建代理
        if (hasMonitorAnnotation(beanClass)) {
            return new Object[] {new MethodInterceptor() {
                @Override
                public Object invoke(MethodInvocation invocation) throws Throwable {
                    long start = System.currentTimeMillis();
                    try {
                        return invocation.proceed();
                    } finally {
                        long duration = System.currentTimeMillis() - start;
                        System.out.println("方法 " + invocation.getMethod().getName() +
                                         " 执行耗时: " + duration + "ms");
                    }
                }
            }};
        }
        return DO_NOT_PROXY;
    }

    private boolean hasMonitorAnnotation(Class<?> beanClass) {
        // 检查类或方法是否有 @Monitor 注解
        return beanClass.isAnnotationPresent(Monitor.class);
    }
}
```

---

## 7. 最佳实践

### 7.1 Scope 选择指南

```java
// 1. 无状态服务 → Singleton
@Service
@Scope("singleton")  // 默认，可以省略
public class UserService {
    // 没有实例变量，只有方法
    public User getUser(Long id) { ... }
}

// 2. 有状态对象 → Prototype
@Component
@Scope("prototype")
public class ShoppingCart {
    private List<Item> items = new ArrayList<>();

    public void addItem(Item item) {
        items.add(item);
    }

    // 每个用户需要自己的购物车
}

// 3. 线程本地数据 → 自定义 Thread Scope
@Component
@Scope("thread")
public class ThreadContext {
    private String currentUserId;
    private String requestId;
    // 每个线程有自己的上下文
}

// 4. 请求数据 → Request Scope（Web 环境）
@Component
@Scope(value = WebApplicationContext.SCOPE_REQUEST,
       proxyMode = ScopedProxyMode.TARGET_CLASS)
public class RequestData {
    private String ipAddress;
    private String userAgent;
}
```

### 7.2 生命周期管理最佳实践

```java
// 1. 优先使用 JSR-250 注解
@Component
public class BestPracticeBean {

    // ✅ 推荐：使用 @PostConstruct
    @PostConstruct
    public void init() {
        // 初始化逻辑
    }

    // ✅ 推荐：使用 @PreDestroy
    @PreDestroy
    public void cleanup() {
        // 清理逻辑
    }

    // ❌ 避免实现 InitializingBean/DisposableBean（与 Spring 耦合）
    // ❌ 避免在构造函数中执行复杂逻辑
}

// 2. 构造函数 vs 属性注入
@Service
public class ServiceExample {

    // ✅ 推荐：构造函数注入（保证依赖不为 null）
    private final UserRepository userRepository;
    private final EmailService emailService;

    public ServiceExample(UserRepository userRepository, EmailService emailService) {
        this.userRepository = userRepository;
        this.emailService = emailService;
    }

    // ✅ 可选：@Value 注入配置
    @Value("${app.max-retry:3}")
    private int maxRetry;
}

// 3. 延迟初始化优化
@Configuration
public class LazyInitConfig {

    @Bean
    @Lazy  // 延迟初始化，第一次使用时才创建
    public ExpensiveService expensiveService() {
        return new ExpensiveService();
    }
}

// 4. 条件化 Bean 创建
@Configuration
public class ConditionalBeanConfig {

    @Bean
    @ConditionalOnProperty(name = "app.cache.enabled", havingValue = "true")
    public CacheService cacheService() {
        return new RedisCacheService();
    }

    @Bean
    @ConditionalOnMissingBean(CacheService.class)
    public CacheService defaultCacheService() {
        return new MemoryCacheService();
    }
}
```

### 7.3 处理 Scope 代理

```java
// 问题：Singleton Bean 注入 Prototype Bean
@Component
public class SingletonService {

    // ❌ 错误：prototypeBean 只会被注入一次
    @Autowired
    private PrototypeBean prototypeBean;

    // ✅ 解决方案1：使用 @Scope 的 proxyMode
    @Autowired
    @Scope(value = "prototype", proxyMode = ScopedProxyMode.TARGET_CLASS)
    private PrototypeBean prototypeBeanWithProxy;

    // ✅ 解决方案2：使用 ObjectFactory
    @Autowired
    private ObjectFactory<PrototypeBean> prototypeBeanFactory;

    // ✅ 解决方案3：使用 ApplicationContext
    @Autowired
    private ApplicationContext applicationContext;

    public void doSomething() {
        // 使用代理，每次调用都获取新实例
        prototypeBeanWithProxy.doWork();

        // 使用工厂
        PrototypeBean bean1 = prototypeBeanFactory.getObject();
        PrototypeBean bean2 = prototypeBeanFactory.getObject();

        // 使用上下文
        PrototypeBean bean3 = applicationContext.getBean(PrototypeBean.class);
    }
}
```

---

## 8. 常见问题与解决方案

### 8.1 Prototype Bean 的销毁问题

```java
// 问题：Spring 不管理 Prototype Bean 的销毁
@Component
@Scope("prototype")
public class PrototypeResource {

    private Resource resource;

    public PrototypeResource() {
        this.resource = new Resource();  // 需要释放的资源
    }

    @PreDestroy  // ❌ 这个方法不会被 Spring 调用！
    public void cleanup() {
        resource.close();
    }
}

// 解决方案1：实现 DisposableBean 并手动管理
@Component
@Scope("prototype")
public class PrototypeResource implements DisposableBean {

    private Resource resource;

    @Override
    public void destroy() {
        if (resource != null) {
            resource.close();
        }
    }
}

// 使用方需要手动销毁
@Service
public class ResourceService {

    @Autowired
    private ApplicationContext context;

    public void useResource() {
        PrototypeResource resource = context.getBean(PrototypeResource.class);
        try {
            // 使用资源
        } finally {
            if (resource instanceof DisposableBean) {
                ((DisposableBean) resource).destroy();
            }
        }
    }
}

// 解决方案2：使用自定义 Scope 管理生命周期
public class CustomPrototypeScope implements Scope {

    private final Map<String, Object> beans = new ConcurrentHashMap<>();
    private final Map<String, Runnable> destructionCallbacks = new ConcurrentHashMap<>();

    @Override
    public Object get(String name, ObjectFactory<?> objectFactory) {
        return beans.computeIfAbsent(name, k -> objectFactory.getObject());
    }

    @Override
    public void registerDestructionCallback(String name, Runnable callback) {
        destructionCallbacks.put(name, callback);
    }

    @Override
    public Object remove(String name) {
        Object bean = beans.remove(name);
        Runnable callback = destructionCallbacks.remove(name);
        if (callback != null) {
            callback.run();  // 执行销毁回调
        }
        return bean;
    }
}
```

### 8.2 循环依赖问题

```java
// 问题：循环依赖
@Component
public class ServiceA {
    @Autowired
    private ServiceB serviceB;
}

@Component
public class ServiceB {
    @Autowired
    private ServiceA serviceA;  // 循环依赖！
}

// 解决方案1：使用 setter 注入（适用于 Singleton）
@Component
public class ServiceA {
    private ServiceB serviceB;

    @Autowired
    public void setServiceB(ServiceB serviceB) {
        this.serviceB = serviceB;
    }
}

// 解决方案2：使用 @Lazy 延迟注入
@Component
public class ServiceB {
    @Autowired
    @Lazy
    private ServiceA serviceA;  // 延迟初始化
}

// 解决方案3：重构设计，避免循环依赖
@Component
public class ServiceA {
    private final EventPublisher eventPublisher;

    public void doSomething() {
        // 发布事件而不是直接调用
        eventPublisher.publishEvent(new SomethingEvent());
    }
}

@Component
public class ServiceB implements ApplicationListener<SomethingEvent> {
    @Override
    public void onApplicationEvent(SomethingEvent event) {
        // 处理事件
    }
}
```

### 8.3 不同 Scope Bean 之间的交互

```java
// 问题：Request/Session Bean 在非 Web 环境中使用
// 解决：条件化创建

@Configuration
public class WebScopeConfig {

    @Bean
    @Scope(
        value = WebApplicationContext.SCOPE_REQUEST,
        proxyMode = ScopedProxyMode.TARGET_CLASS
    )
    @ConditionalOnWebApplication(type = ConditionalOnWebApplication.Type.SERVLET)
    public RequestScopedBean requestScopedBean() {
        return new RequestScopedBean();
    }

    @Bean
    @Scope(
        value = WebApplicationContext.SCOPE_SESSION,
        proxyMode = ScopedProxyMode.TARGET_CLASS
    )
    @ConditionalOnWebApplication(type = ConditionalOnWebApplication.Type.SERVLET)
    public SessionScopedBean sessionScopedBean() {
        return new SessionScopedBean();
    }
}

// 问题：Prototype Bean 在 Singleton 中的使用
@Service
public class SingletonService {

    // 方案1：每次通过上下文获取
    @Autowired
    private ApplicationContext context;

    public void processWithPrototype() {
        PrototypeBean bean = context.getBean(PrototypeBean.class);
        bean.process();
    }

    // 方案2：使用方法注入（不推荐，性能差）
    protected abstract PrototypeBean createPrototypeBean();

    // 方案3：使用 Provider（推荐）
    @Autowired
    private Provider<PrototypeBean> prototypeBeanProvider;

    public void processWithProvider() {
        PrototypeBean bean = prototypeBeanProvider.get();
        bean.process();
    }
}
```

### 8.4 性能优化建议

```java
// 1. 合理使用延迟加载
@Configuration
public class PerformanceConfig {

    // 启动时不需要的服务可以延迟加载
    @Bean
    @Lazy
    public HeavyService heavyService() {
        return new HeavyService();  // 初始化耗时
    }

    // 必须提前启动的服务不要延迟加载
    @Bean
    public CriticalService criticalService() {
        return new CriticalService();  // 立即初始化
    }
}

// 2. 避免在初始化时执行耗时操作
@Component
public class AsyncInitBean {

    @Autowired
    private AsyncTaskExecutor taskExecutor;

    @PostConstruct
    public void init() {
        // 快速完成基本初始化
        doQuickInit();

        // 耗时操作异步执行
        taskExecutor.execute(this::doHeavyInit);
    }

    private void doQuickInit() {
        // 基本初始化
    }

    private void doHeavyInit() {
        // 耗时初始化
    }
}

// 3. 使用 BeanPostProcessor 批量处理
@Component
public class BatchProcessor implements BeanPostProcessor {

    @Override
    public Object postProcessAfterInitialization(Object bean, String beanName) {
        // 批量验证，而不是在每个 Bean 的 init 方法中
        if (bean instanceof Validatable) {
            validateBean((Validatable) bean);
        }
        return bean;
    }

    private void validateBean(Validatable bean) {
        // 批量验证逻辑
    }
}
```

## 总结

1. **理解 Scope 的本质**：Scope 决定了 Bean 的实例化和生命周期管理方式
2. **合理选择 Scope**：根据业务需求选择合适的 Scope，避免误用
3. **生命周期管理**：推荐使用 JSR-250 注解，避免与框架强耦合
4. **注意线程安全**：Singleton Bean 必须考虑线程安全问题
5. **处理特殊场景**：循环依赖、不同 Scope 交互等特殊场景需要特殊处理
6. **性能考虑**：合理使用延迟加载和异步初始化优化性能

通过深入理解这些知识点，可以更好地使用 Spring 的 IoC 容器，编写出更健壮、高效的应用程序。