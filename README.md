# tiny
### c语言实现的简单服务器，来源于CSAPP.

### 分支
- master ：主分支，稳定的版本
- devolop :测试分支，一些新特性的测试

### 暂时的特性
- 支持HTTP 1.0
- 支持GET请求
- 支持CGI
- 并发处理请求
- 支持POST请求

### 待完善
- 实现负载均衡（缓冲区）

## 代码剖析
[c语言实现一个小而全的服务器](http://www.sshoop.top/blog/article/23)

### 简单的测试
1. git clone
2. gcc -o tiny tiny.c csapp.c -lpthread
3. ./tiny 2314
4. 打开浏览器输入 http:localhost:2314/
