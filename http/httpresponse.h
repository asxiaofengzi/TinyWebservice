#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include <unordered_map>
#include <fcntl.h>	   // open
#include <unistd.h>	   // close
#include <sys/stat.h>  // stat
#include <sys/mman.h>  // mmap, munmap

#include "../buffer/buffer.h"
#include "../log/log.h"

class HttpResponse {
private:
	void AddStateLine_(Buffer& buff); // 添加状态行
	void AddHeader_(Buffer& buff); // 添加头部
	void AddContent_(Buffer& buff); // 添加内容

	void ErrorHtml_(); // 错误页面
	std::string GetFileType_(); // 获取文件类型

	int code_; // 状态码
	bool isKeepAlive_; // 是否保持连接

	std::string path_; // 路径
	std::string srcDir_; // 源目录

	char* mmFile_; // 文件映射地址
	struct stat mmFileStat_; // 文件状态

	static const std:: unordered_map<std::string, std::string> SUFFIX_TYPE; // 后缀类型集
	static const std:: unordered_map<int, std::string> CODE_STATUS; // 状态码集
	static const std:: unordered_map<int, std::string> CODE_PATH; // 状态码对应路径集

public:
	HttpResponse(); // 构造函数
	~HttpResponse(); // 析构函数

	void Init(const std::string& srcDir, std::string& path, bool isKeepAlive = false, int code = -1); // 初始化
	void MakeResponse(Buffer& buff); // 响应
	void UnmapFile(); // 解除映射
	char* File(); // 文件
	size_t FileLen() const; // 文件长度
	void ErrorContent(Buffer& buff, std::string message); // 错误内容
	int Code() const { return code_; }; // 状态码
};

#endif //HTTP_RESPONSE_H