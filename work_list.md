<!--
 * @Author: Amadeus
 * @Date: 2024-04-17 16:56:42
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-04-19 18:55:01
 * @FilePath: /Amadeus/work_list.md
 * @Description: 
-->
# 第一周期工作
- 搭建项目框架
- 实现RTMP-flv推拉流

## 启动配置模块(4.19完成)
- 功能：
    1. 可方便的在全局代码中读取需要的配置
    2. 从json文件读取配置服务相关配置
    3. 从命令行读取配置seastar参数和服务相关配置
    
- 分析：
    - 对于功能i，使用单例模式创建一个`global_settings`类，类中写死程序需要的所有配置项，在程序开始运行时对单例初始化，之后调用相关接口获取所需的配置。
    - 对于功能ii，使用nlohmann::json实现`global_settings`读取json文件的功能。
    - 对于功能iii，由于seatar框架使用`boost::program_options`实现命令行参数的读取，因此`global_settings`类应该兼容与`boost::program_options`互转的功能。

- 设计：
    - `global_settings`由多个`element`类组成，每个`element`类对应一个配置项，具备和`boost::program_option`、json互转的功能。
    - `global_settings`类管理所有的`element`类，提供全局的配置读取接口。
    - json配置文件的绝对路径由命令行给出

## 日志模块