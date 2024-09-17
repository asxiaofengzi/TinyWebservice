#include "httpresponse.h"

using namespace std;

// 状态码对应状态信息    将文件扩展名映射到相应的 MIME 类型
const unordered_map<string, string> HttpResponse::SUFFIX_TYPE = {
	{ ".html",  "text/html" },
	{ ".xml",   "text/xml" },
	{ ".xhtml", "application/xhtml+xml" },
	{ ".txt",   "text/plain" },
	{ ".rtf",   "application/rtf" },
	{ ".pdf",   "application/pdf" },
	{ ".word",  "application/nsword" },
	{ ".png",   "image/png" },
	{ ".gif",   "image/gif" },
	{ ".jpg",   "image/jpeg" },
	{ ".jpeg",  "image/jpeg" },
	{ ".au",    "audio/basic" },
	{ ".mpeg",  "video/mpeg" },
	{ ".mpg",   "video/mpeg" },
	{ ".avi",   "video/x-msvideo" },
	{ ".gz",    "application/x-gzip" },
	{ ".tar",   "application/x-tar" },
	{ ".css",   "text/css "},
	{ ".js",    "text/javascript "},
};

// 状态码对应状态信息   将状态码映射到相应的状态信息
const unordered_map<int, string> HttpResponse::CODE_STATUS = {
	{ 200, "OK" },
	{ 400, "Bad Request" },
	{ 403, "Forbidden" },
	{ 404, "Not Found" },
};

// 状态码对应路径信息   将状态码映射到相应的路径
const unordered_map<int, string> HttpResponse::CODE_PATH = {
	{ 400, "/400.html" },
	{ 403, "/403.html" },
	{ 404, "/404.html" },
};

// 构造函数
HttpResponse::HttpResponse() {
	code_ = -1; // 状态码
	path_ = srcDir_ = ""; // 路径和源目录
	isKeepAlive_ = false; // 是否保持连接
	mmFile_ = nullptr; // 文件映射地址
	mmFileStat_ = { 0 }; // 文件状态
}

// 析构函数
HttpResponse::~HttpResponse() {
	UnmapFile(); // 解除映射
}

// 初始化
void HttpResponse::Init(const string& srcDir, string& path, bool isKeepAlive, int code) {
	assert(srcDir != ""); // 断言源目录不为空
	if(mmFile_) { UnmapFile(); } // 如果文件映射地址不为空，则解除映射
	code_ = code; // 状态码
	isKeepAlive_ = isKeepAlive; // 是否保持连接
	path_ = path; // 路径
	srcDir_ = srcDir; // 源目录
	mmFile_ = nullptr; // 文件映射地址
	mmFileStat_ = { 0 }; // 文件状态
}

// 响应
void HttpResponse::MakeResponse(Buffer& buff) {
	// 判断请求的资源文件
	// 如果请求的资源文件不存在或者是一个目录，则状态码为404
	/*stat 是一个 POSIX 系统调用，用于获取文件的状态信息。它返回一个结构体，其中包含文件的各种属性，如大小、权限、最后修改时间等。
	在 C++ 中，stat 通常通过 <sys/stat.h> 头文件提供。
	stat 函数返回 0 表示成功，返回 -1 表示失败。

	stat 结构体的常用成员
	st_size：文件大小（以字节为单位）。
	st_mode：文件的模式（包括文件类型和权限）。
	st_mtime：文件的最后修改时间。*/
	if(stat((srcDir_ + path_).data(), &mmFileStat_) < 0 || S_ISDIR(mmFileStat_.st_mode)) {
		code_ = 404; // 未找到
	}
	// 如果请求的资源文件不可读，则状态码为403
	// S_IROTH 是一个宏定义，用于表示文件的“其他用户”读权限（即非所有者和非组成员的读权限）
	else if(!(mmFileStat_.st_mode & S_IROTH)) {
		code_ = 403; // 禁止访问
	}
	else if(code_ == -1) {
		code_ = 200; // 成功
	}
	ErrorHtml_(); // 错误页面
	AddStateLine_(buff); // 添加状态行
	AddHeader_(buff); // 添加头部
	AddContent_(buff); // 添加内容
}

// 文件
char* HttpResponse::File() {
	return mmFile_; // 返回文件映射地址
}

// 文件长度
size_t HttpResponse::FileLen() const {
	return mmFileStat_.st_size; // 返回文件大小
}

// 错误页面
void HttpResponse::ErrorHtml_() {
	// 如果状态码对应的路径存在，则将路径设置为对应的路径，并获取文件状态
	if(CODE_PATH.count(code_) == 1) {
		path_ = CODE_PATH.find(code_)->second; // 获取路径
		stat((srcDir_ + path_).data(), &mmFileStat_); // 获取文件状态
	}
}

// 添加状态行
void HttpResponse::AddStateLine_(Buffer& buff) {
	string status;
	// 如果状态码对应的状态信息存在，则将状态信息设置为对应的状态信息
	if(CODE_STATUS.count(code_) == 1) {
		status = CODE_STATUS.find(code_)->second; // 获取状态信息
	}
	else {
		code_ = 400; // 状态码为400
		status = CODE_STATUS.find(400)->second; // 状态信息为Bad Request
	}
	buff.Append("HTTP/1.1 " + to_string(code_) + " " + status + "\r\n"); // 添加状态行
}

// 添加头部
void HttpResponse::AddHeader_(Buffer& buff) {
	buff.Append("Connection: "); // 连接
	if(isKeepAlive_) {
		buff.Append("keep-alive\r\n"); // 保持连接
		buff.Append("keep-alive: max=6, timeout=120\r\n"); // 保持连接的最大次数和超时时间
	}
	else {
		buff.Append("close\r\n"); // 关闭连接
	}
	buff.Append("Content-type: " + GetFileType_() + "\r\n"); // 内容类型
}

// 添加内容
void HttpResponse::AddContent_(Buffer& buff) {
	// O_RDONLY 是一个宏定义，用于表示以只读模式打开文件
	/*调用 open 函数：int fileDescriptor = open(filePath, O_RDONLY); 以只读模式打开文件，并返回文件描述符。
	检查返回值：如果 open 返回 -1，表示打开文件失败；否则表示成功。
	关闭文件：使用 close(fileDescriptor); 关闭文件。*/
	int srcFd = open((srcDir_ + path_).data(), O_RDONLY); // 打开文件
	if(srcFd < 0) { // 如果文件打开失败
		ErrorContent(buff, "File NotFound!"); // 错误内容
		return; // 返回
	}

	// 将文件映射到内存提高文件的访问速度  MAP_PRIVATE 建立一个写入时拷贝的私有映射
	LOG_DEBUG("file path %s", (srcDir_ + path_).data());
	/*mmap 函数：

	mmap 是一个 POSIX 系统调用，用于将文件或设备映射到内存地址空间。
	函数原型：
	void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset);
	参数解释：
	void* addr：映射的起始地址，通常为 0，表示由系统选择地址。
	size_t length：映射的长度，这里是 mmFileStat_.st_size，表示文件的大小。
	int prot：内存保护标志，这里是 PROT_READ，表示映射区域是只读的。
	int flags：映射标志，这里是 MAP_PRIVATE，表示创建一个私有的写时复制映射。
	int fd：文件描述符，这里是 srcFd，表示要映射的文件。
	off_t offset：文件偏移量，这里是 0，表示从文件开始处映射。
	返回值：

	mmap 返回一个指向映射区域的指针。
	如果映射失败，返回 MAP_FAILED，通常为 (void*)-1。
	类型转换：

	返回的指针被强制转换为 int* 类型，并赋值给 mmRet。*/
	int* mmRet = (int*)mmap(0, mmFileStat_.st_size, PROT_READ, MAP_PRIVATE, srcFd, 0); // 将文件映射到内存
	if(*mmRet == -1) { // 如果文件映射失败
		ErrorContent(buff, "File NotFound!"); // 错误内容
		return; // 返回
	}
	mmFile_ = (char*)mmRet; // 文件映射地址
	close(srcFd); // 关闭文件
	buff.Append("Content-length: " + to_string(mmFileStat_.st_size) + "\r\n\r\n"); // 内容长度
}

// 解除映射
void HttpResponse::UnmapFile() { 
	if(mmFile_) { // 如果文件映射地址不为空
		munmap(mmFile_, mmFileStat_.st_size); // 解除映射
		mmFile_ = nullptr; // 文件映射地址为空
	}
}

// 错误内容
void HttpResponse::ErrorContent(Buffer& buff, string message) {
	string body; // 内容
	string status; // 状态
	body += "<html><title>Error</title>"; // 标题
	body += "<body bgcolor=\"ffffff\">"; // 背景颜色
	body += to_string(code_) + " : " + message; // 状态码和消息
	body += "<p>" + message + "</p>"; // 消息
	body += "<hr><em>WebServer</em></body></html>"; // 服务器
	status = CODE_STATUS.find(code_)->second; // 状态
	buff.Append("HTTP/1.1 " + to_string(code_) + " " + status + "\r\n"); // 状态行
	buff.Append("Content-type: text/html\r\n"); // 内容类型
	buff.Append("Connection: close\r\n"); // 关闭连接
	buff.Append("Content-length: " + to_string(body.size()) + "\r\n\r\n"); // 内容长度
	buff.Append(body); // 内容
}

// 获取文件类型
string HttpResponse::GetFileType_() {
	string::size_type idx = path_.find_last_of('.'); // 查找文件扩展名
	if(idx == string::npos) { // 如果没有找到
		return "text/plain"; // 返回文本类型
	}
	string suffix = path_.substr(idx); // 获取文件扩展名
	if(SUFFIX_TYPE.count(suffix) == 1) { // 如果文件扩展名存在
		return SUFFIX_TYPE.find(suffix)->second; // 返回文件类型
	}
	return "text/plain"; // 返回文本类型
}