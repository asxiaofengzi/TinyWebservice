#ifndef HTTPREUQEST_H
#define HTTPREUQEST_H

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <regex> // 正则表达式
#include <errno.h>
#include <mysql.h> //mysql

#include "../buffer/buffer.h"
#include "../log/log.h"
#include "../pool/sqlconnpool.h"

class HttpRequest
{
private:
    bool ParseRequestLine_(const std::string &line); // 处理请求行
    void ParseHeader_(const std::string &line);      // 处理请求头
    void ParseBody_(const std::string &line);        // 处理请求体

    void ParsePath_();           // 处理请求路径
    void ParsePost_();           // 处理Post事件
    void ParseFromUrlencoded_(); // 从url中解析编码

    static bool UserVerify(conse std::string &name, const std::string &pwd, bool isLogin); // 用户验证

    PARSE_STATE state_;
    std::string method_, path_, version_, body_;
    std::unordered_map<std::string, std::string> header_;
    std::unordered_map<std::string, std::string> post_;

    static const std::unordered_set<std::string> DEFAULT_HTML;
    static const std::unordered_map<std::string, int> DEFAULT_HTML_TAG;
    static int ConverHex(char ch); // 16进制转换为10进制
public:
    // 定义 PARSE_STATE 枚举，表示解析状态，包括请求行、请求头、请求体和完成。
    enum PARSE_STATE
    {
        REQUEST_LINE,
        HEADERS,
        BODY,
        FINISH,
    };

    HttpRequest() { Init(); } // 构造函数调用 Init 方法初始化对象。
    ~HttpRequest() = default; // 析构函数使用默认实现。

    void Init();              // Init 方法初始化对象。
    bool parse(Buffer &buff); // parse 方法解析缓冲区中的数据。

    // 用于获取请求的路径、方法、版本和 POST 数据。
    std::string path() const;
    std::string &path();
    std::string method() const;
    std::string version() const;
    std::string GetPost(const std::string &key) const;
    std::string GetPost(const char *key) const;

    bool IsKeepAlive() const; // 检查连接是否保持活动状态。
};

#endif // HTTPREUQEST_H