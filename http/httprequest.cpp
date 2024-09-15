#include "httprequest.h"
using namespace std;

// 网页名称，和一般的前端跳转不同，这里需要将请求信息放到后端来验证一遍再上传
/*定义并初始化了 HttpRequest 类的一个静态常量成员 DEFAULT_HTML。
DEFAULT_HTML 是一个 unordered_set<string> 类型的集合，包含了一些默认的 HTML 路径。*/
const unordered_set<string> HttpRequest::DEFAULT_HTML{
    "/index",
    "/register",
    "/login",
    "/welcome",
    "/video",
    "/picture",
};

// 登录/注册
// 定义并初始化了 HttpRequest 类的一个静态常量成员 DEFAULT_HTML_TAG。
const unordered_map<string &> HttpRequest::DEFAULT_HTML_TAG{
    {"/login.html", 1}, {"/register.html", 0}};

// 初始化操作，一些清零操作
void HttpRequest::Init()
{
    state_ = REQUEST_LINE;                   // 初始状态
    method_ = path_ = version_ = body_ = ""; // 初始化 method_、path_、version_ 和 body_ 为空字符串。
    header_.clear();                         // 清空 header_。
    post_.clear();                           // 清空 post_。
}

// 解析处理
bool HttpRequest::parse_(Buffer &buff)
{
    const char END[] = "\r\n";     // 定义了一个字符串 END，内容为 "\r\n"。
    if (buff.ReadableBytes() == 0) // 如果 buff 中没有可读的字节，返回 false。
        return false;
    // 读取数据开始
    while (buff.ReadableBytes() && state_ != FINISH)
    {
        // 从 buff 中的读指针开始到读指针结束，这块区域是未读取的数据并去除 "\r\n"，返回有效数据的行末指针。
        const char *lineend = search(buff.Peek(), buff.BeginWriteConst(), END, END + 2); // 查找 buff 中的 "\r\n"。
        string line(buff.Peek(), lineend);                                               // 从 buff 中读取数据到 line 中。构造函数 string 将从 buff.Peek() 指向的位置开始，直到 lineend 指向的位置为止，创建一个新的字符串对象。
        switch (state_)
        {
        case REQUEST_LINE:
            // 解析错误
            if (!ParseRequestLine_(line))
            { // 调用 ParseRequestLine_ 方法解析请求行，如果解析失败，返回 false。
                return false;
            }
            ParsePath_(); // 解析路径
            break;
        case HEADERS:
            ParseHeader_(line); // 调用 ParseHeader_ 方法解析请求头。
            if (buff.ReadableBytes() <= 2)
            {                    // 如果 buff 中的可读字节数小于等于 2，说明是 GET 请求，后面为 "\r\n"。
                state_ = FINISH; // 状态转换为 FINISH。
            }
            break;
        case BODY:
            ParseBody_(line); // 调用 ParseBody_ 方法解析请求体。
            break;
        default:
            break;
        }
        if (linnend == buff.BeginWrite())
        {                       // 如果 lineend 指向 buff 的 BeginWrite()，说明读完了。
            buff.RetrieveAll(); // 重置 buff。
            break;
        }
        buff.RetrieveUntil(lineend + 2); // 跳过回车换行。
    }
    LOG_DEBUG("[%s], [%s], [%s]", method_.c_str(), path_.c_str(), version_.c_str());
    return true;
}

// 解析请求行
bool HttpRequest::ParseRequestLine_(const string &line)
{
    /*正则表达式 ^([^ ]*) ([^ ]*) HTTP/([^ ]*)$ 可以分解为以下几个部分：
    ^：表示匹配字符串的开始。

    ( 和 )：圆括号用于定义一个捕获组。捕获组会保存匹配的子字符串，以便后续引用或处理。
    [^ ]：方括号内的 ^ 表示取反，[^ ] 表示匹配任何不是空格的字符。
    *：星号表示匹配前面的模式零次或多次。因此，[^ ]* 表示匹配零个或多个非空格字符。

    ([^ ]*)：第一个捕获组，匹配一个或多个非空格字符。这通常对应于 HTTP 请求方法（例如，GET、POST 等）。
    ([^ ]*)：匹配一个空格，后跟第二个捕获组，匹配一个或多个非空格字符。这通常对应于请求的路径（例如，/index.html）。
    HTTP/：匹配字符串 HTTP/，这是 HTTP 请求行的固定部分。
    ([^ ]*)：第三个捕获组，匹配一个或多个非空格字符。这通常对应于 HTTP 版本（例如，1.1、2.0 等）。
    $：表示匹配字符串的结束。
    通过这个正则表达式，我们可以解析出 HTTP 请求行中的三个主要部分：请求方法、请求路径和 HTTP 版本。*/
    regex pattern("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$"); // 定义了一个正则表达式 pattern ，用于匹配请求行。
    // smatch 是标准库 <regex> 中定义的一个类型，用于存储正则表达式匹配结果。
    smatch Match; // 定义了一个 smatch 对象 Match，用于存储匹配结果。
    if (regex_match(line, Match, pattern))
    {                        // 如果 line 匹配正则表达式 pattern ，将匹配结果存储到 Match 中。
        method_ = Match[1];  // 将 Match 中第一个括号中的内容存储到 method_ 中。
        path_ = Match[2];    // 将 Match 中第二个括号中的内容存储到 path_ 中。
        version_ = Match[3]; // 将 Match 中第三个括号中的内容存储到 version_ 中。
        state_ = HEADERS;    // 状态转换为 HEADERS。
        return true;
    }
    LOG_ERROR("RequestLine Error"); // 如果匹配失败，打印错误日志。
    return false;
}

// 解析路径，统一一下 path 名称，方便后面解析资源
void HttpRequest::ParsePath_()
{
    if (path_ == "/")
    { // 如果 path_ 为 "/"，将 path_ 设置为 "/index.html"。
        path_ = "/index.html";
    }
    else
    { // 否则，如果 path_ 在 DEFAULT_HTML 中，将 path_ 后面添加 ".html"。
        if (DEFAULT_HTML.find(path_) != DEFAULT_HTML.end())
        {
            path_ += ".html";
        }
    }
}

// 解析请求头
void HttpRequest::ParseHeader_(const string &line)
{
    /*正则表达式 ^([^:]*): ?(.*)$ 可以分解为以下几个部分：
    ^：表示匹配字符串的开始。

    ( 和 )：圆括号用于定义一个捕获组。捕获组会保存匹配的子字符串，以便后续引用或处理。
    [^:]*：方括号内的 ^ 表示取反，[^:]* 表示匹配任何不是冒号的字符，* 表示匹配前面的模式零次或多次。
    :：匹配一个冒号。
    ?：匹配前面的模式零次或一次。
    (.*)：匹配任意字符零次或多次。这通常对应于冒号后面的值。
    $：表示匹配字符串的结束。
    通过这个正则表达式，我们可以解析出 HTTP 请求头中的键值对。*/
    regex pattern("^([^:]*): ?(.*)$"); // 定义了一个正则表达式 pattern ，用于匹配请求头。
    smatch Match;                      // 定义了一个 smatch 对象 Match，用于存储匹配结果。
    if (regex_match(line, Match, pattern))
    {                                 // 如果 line 匹配正则表达式 pattern ，将匹配结果存储到 Match 中。
        header_[Match[1]] = Match[2]; // 将 Match 中第一个括号中的内容作为键，第二个括号中的内容作为值，存储到 header_ 中。
    }
    else
    {                  // 如果匹配失败，说明首部行匹配完了，状态变化。
        state_ = BODY; // 状态转换为 BODY。
    }
}

// 解析请求体
void HttpRequest::ParseBody_(const string &line)
{
    body_ = line;                                            // 将 line 存储到 body_ 中。
    ParsePost_();                                            // 调用 ParsePost_ 方法解析 POST 请求体。
    state_ = FINISH;                                         // 状态转换为 FINISH。
    LOG_DEBUG("Body:%s, len:%d", line.c_str(), line.size()); // 打印日志。
}

// 16 进制转换为 10 进制
int HttpRequest::ConverHex(char ch)
{
    if (ch >= 'A' && ch <= 'F') // 如果 ch 大于等于 'A' 并且小于等于 'F'，返回 ch - 'A' + 10。
        return ch - 'A' + 10;
    if (ch >= 'a' && ch <= 'f') // 如果 ch 大于等于 'a' 并且小于等于 'f'，返回 ch - 'a' + 10。
        return ch - 'a' + 10;
    return ch; // 否则，返回 ch。
}

// 处理 POST 请求
void HttpRequest::ParsePost_()
{
    if (method_ == "POST" && header_["Content-Type"] == "application/x-www-form-urlencoded")
    {                           // 如果 method_ 为 "POST" 并且 Content-Type 为 "application/x-www-form-urlencoded"。
        ParseFromUrlencoded_(); // 调用 ParseFromUrlencoded_ 方法解析 POST 请求体。
        if (DEFAULT_HTML_TAG.count(path_))
        {                                                   // 如果 path_ 在 DEFAULT_HTML_TAG 中。
            int tag = DEFAULT_HTML_TAG.find(path_)->second; // 获取 path_ 对应的值。
            LOG_DEBUG("Tag:%d", tag);                       // 打印日志。
            if (tag == 0 || tag == 1)
            {                              // 如果 tag 为 0 或 1。
                bool isLogin = (tag == 1); // 如果 tag 为 1，isLogin 为 true；否则，isLogin 为 false。
                if (UserVerify(post_["username"], post_["password"], isLogin))
                {                            // 调用 UserVerify 方法验证用户名和密码。
                    path_ = "/welcome.html"; // 如果验证成功，将 path_ 设置为 "/welcome.html"。
                }
                else
                { // 否则，将 path_ 设置为 "/error.html"。
                    path_ = "/error.html";
                }
            }
        }
    }
}

// 从 URL 中解析编码
// 这里还需要继续看一下，在写完之后对这里跑一下
void HttpRequest::ParseFromUrlencoded_()
{
    if (body_.size() == 0)
    { // 如果 body_ 为空，返回。
        return;
    }

    string key, value;    // 定义 key 和 value。
    int num = 0;          // 定义 num。
    int n = body_.size(); // 获取 body_ 的大小。
    int i = 0, j = 0;     // 定义 i 和 j。

    for (; i < n; i++)
    {                       // 遍历 body_。
        char ch = body_[i]; // 获取 body_ 中的字符。
        switch (ch)         // switch 判断。
        {
        case '=':                         // 如果 ch 为 '='。
            key = body_.substr(j, i - j); // 获取 key。
            j = i + 1;                    // j 设置为 i + 1。
            break;
        case '+':           // 如果 ch 为 '+'。
            body_[i] = ' '; // 将 body_ 中的 '+' 替换为 ' '。
            break;
        case '%':                                                         // 如果 ch 为 '%'。
            num = ConverHex(body_[i + 1]) * 16 + ConverHex(body_[i + 2]); // 将 body_ 中的 16 进制转换为 10 进制。
            body_[i + 2] = num % 10 + '0';                                // 将 num 的个位数转换为字符。
            body_[i + 1] = num / 10 + '0';                                // 将 num 的十位数转换为字符。
            i += 2;                                                       // i 加 2。
            break;
        case '&':                                             // 如果 ch 为 '&'。
            value = body_.substr(j, i - j);                   // 获取 value。
            j = i + 1;                                        // j 设置为 i + 1。
            post_[key] = value;                               // 将 key 和 value 存储到 post_ 中。
            LOG_DEBUG("%s = %s", key.c_str(), value.c_str()); // 打印日志。
            break;
        default: // 默认情况。
            break;
        }
    }
    assert(j <= i); // 断言 j 小于等于 i。
    if (post_.count(key) == 0 && j < i)
    {                                   // 如果 post_ 中没有 key 并且 j 小于 i。
        value = body_.substr(j, i - j); // 获取 value。
        post_[key] = value;             // 将 key 和 value 存储到 post_ 中。
    }
}

// 用户验证
bool HttpRequest::UserVerify(const string &name, const string &pwd, bool isLogin)
{
    if (name == "" || pwd == "")
    {
        return false;
    } // 如果 name 或 pwd 为空，返回 false。
    LOG_INFO("Verify name:%s pwd:%s", name.c_str(), pwd.c_str());
    MYSQL *sql;                                 // 定义 MYSQL 对象 sql。
    SqlConnRAII(&sql, SqlConnPool::Instance()); // 创建 SqlConnRAII 对象，获取数据库连接。
    assert(sql);

    bool flag = false;
    unsigned int j = 0;
    char order[256] = {0}; // 定义 order 数组，大小为 256。
    /*MYSQL_FIELD 是 MySQL C API 中的一个结构体类型，用于描述结果集中字段（列）的元数据。它包含字段的名称、类型、长度等信息。*/
    MYSQL_FIELD *fields = nullptr; // 定义 MYSQL_FIELD 对象 fields。
    // MYSQL_RES 是 MySQL C API 中的一个结构体类型，用于表示查询结果集。当你执行一个查询（例如 SELECT 语句）时，MySQL 会返回一个 MYSQL_RES 对象，包含查询的结果。
    MYSQL_RES *res = nullptr; // 定义 MYSQL_RES 对象 res。

    if (!isLogin)
    {
        flag = true;
    } // 如果不是登录，flag 设置为 true。
    /* 查询用户及密码 */
    /*snprintf是一个C语言中的函数，用于将格式化的数据写入字符串中。它类似于printf函数，但是不是将结果打印到标准输出，而是将结果写入指定的字符串缓冲区。

    snprintf函数的原型如下：

    参数说明：

    str：指向要写入的字符串缓冲区的指针。
    size：要写入的最大字符数（包括终止符）。
    format：格式化字符串，指定要写入的数据的格式。
    ...：可变参数，根据format字符串中的格式指定要写入的数据。
    snprintf函数会根据指定的格式将数据写入字符串缓冲区，并返回写入的字符数（不包括终止符）。如果写入的字符数超过了指定的最大字符数，那么多余的字符将被截断。*/
    snprintf(order, 256, "SELECT username, password FROM user WHERE username='%s' LIMIT 1", name.c_str());
    LOG_DEBUG("%s", order);

    if (mysql_query(sql, order))
    { // 执行 SQL 查询。
        /*已使用 /explain (重新运行而不使用)
        根据你提供的代码片段，mysql_free_result 是一个函数调用。它是用于释放 MySQL 查询结果的函数。

        在使用 MySQL 进行数据库查询时，通常会使用 mysql_query 函数执行查询，并通过 mysql_store_result 函数获取查询结果。当你完成了对查询结果的处理后，需要使用 mysql_free_result 函数来释放内存资源，以避免内存泄漏。

        这个函数的作用是释放 mysql_store_result 函数返回的结果集对象所占用的内存。通过调用 mysql_free_result 函数，你可以确保在不再需要查询结果时，释放相关的内存资源。*/
        mysql_free_result(res); // 释放结果集。
        return false;
    }
    res = mysql_store_result(sql);    // 存储结果。
    j = mysql_num_fields(res);        // 获取结果集中的字段数。
    fields = mysql_fetch_fields(res); // 获取结果集中的字段。

    while (MYSQL_ROW row = mysql_fetch_row(res))
    { // 遍历结果集。
        LOG_DEBUG("MYSQL ROW: %s %s", row[0], row[1]);
        string password(row[1]); // 获取密码。
        /* 注册行为 且 用户名未被使用*/
        if (isLogin)
        {
            if (pwd == password)
            {
                flag = true;
            } // 如果密码正确，flag 设置为 true。
            else
            {
                flag = false;
                LOG_INFO("pwd error!");
            }
        }
        else
        {
            flag = false;
            LOG_INFO("user used!");
        }
    }
    mysql_free_result(res);

    /* 注册行为 且 用户名未被使用*/
    if (!isLogin && flag == true)
    {
        LOG_DEBUG("regirster!");
        bzero(order, 256); // 将 order 数组清零。
        snprintf(order, 256, "INSERT INTO user(username, password) VALUES('%s','%s')", name.c_str(), pwd.c_str());
        LOG_DEBUG("%s", order);
        if (mysql_query(sql, order))
        {
            LOG_DEBUG("Insert error!");
            flag = false;
        }
        flag = true;
    }
    // SqlConnPool::Instance()->FreeConn(sql);
    LOG_DEBUG("UserVerify success!!");
    return flag;
}

/*在 C++ 中，在成员函数名后面加上 const 关键字表示该成员函数是一个常量成员函数（const member function）。
这意味着该函数不会修改所属对象的状态，也就是说，它不会修改任何成员变量的值。
常量成员函数可以在常量对象上调用，而非常量成员函数则不能。*/
std::string HttpRequest::path() const
{
    return path_;
}

// 因为要对path_进行修改，所以这里返回的是path_的引用，可以修改path_的值。
std::string &HttpRequest::path()
{ // 这里的 path() 函数返回的是 path_ 的引用，可以修改 path_ 的值。
    return path_;
}

std::string HttpRequest::method() const
{
    return method_;
}

std::string HttpRequest::version() const
{
    return version_;
}

// 用于获取请求的路径、方法、版本和 POST 数据。
std::string HttpRequest::GetPost(const std::string &key) const
{
    assert(key != "");
    if (post_.count(key) == 1)
    {                                   // 如果 post_ 中有 key。
        return post_.find(key)->second; // 返回 key 对应的值。
    }
    return "";
}

// 用于获取请求的路径、方法、版本和 POST 数据。
std::string HttpRequest::GetPost(const char *key) const
{
    assert(key != nullptr);
    if (post_.count(key) == 1)
    {
        return post_.find(key)->second;
    }
    return "";
}

// 检查连接是否保持活动状态。
bool HttpRequest::IsKeepAlive() const
{
    if (header_.count("Connection") == 1)
    {
        return header_.find("Connection")->second == "keep-alive" && version_ == "1.1";
    }
    return false;
}