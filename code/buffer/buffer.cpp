#include "buffer.h"

// Buffer类的初始化，把读写下标初始化，vector<char>初始化
Buffer::Buffer(int initBuffSize) : buffer_(initBuffSize), readPos_(0), writePos_(0) {}

// 可写的数量：buffer大小 - 写下标
size_t Buffer::WritableBytes() const
{
  return buffer_.size() - writePos_;
}

// 可读的数量：写下标 - 读下标
size_t Buffer::ReadableBytes() const
{
  return writePos_ - readPos_;
}

// 可预留空间：已经读过的就没用了，等于读下标
size_t Buffer::PrependableBytes() const
{
  return readPos_;
}

// 读指针的位置
const char *Buffer::Peek() const
{
  return &buffer_[readPos_];
}

// 确保可写的长度
void Buffer::EnsureWriteable(size_t len)
{
  if (len > WritableBytes())
  {
    MakeSpace_(len);
  }
  assert(len <= WritableBytes());
}

// 移动写下标，在Append中使用
void Buffer::HasWritten(size_t len)
{
  writePos_ += len;
}

// 读取len长度，移动读下标
void Buffer::Retrieve(size_t len)
{
  readPos_ += len;
}

// 读取到end位置
void Buffer::RetrieveUntil(const char *end)
{
  assert(Peek() <= end);
  Retrieve(end - Peek()); // end指针 - 读指针  长度
}

// 取出所有数据，buffer归零，读写下标归零，在别的函数中会用到
void Buffer::RetrieveAll()
{
  memset(&buffer_[0], 0, buffer_.size()); // 自 C11 标准起，bzero 已经被标记为不推荐使用，因为它的名称可能会与宏定义冲突，并且存在更安全的替代函数，如 memset。
  readPos_ = 0;
  writePos_ = 0;
}

// 取出剩余可读的str
std::string Buffer::RetrieveAllToStr()
{
  std::string str(Peek(), ReadableBytes());
  RetrieveAll();
}

// 写指针的位置
const char *Buffer::BeginWriteConst() const
{
  return &buffer_[writePos_];
}

char *Buffer::BeginWrite()
{
  return &buffer_[writePos_];
}

// 添加str到缓冲区
void Buffer::Append(const std::string &str)
{
  Append(str.c_str(), str.size());
}

void Buffer::Append(const char *str, size_t len)
{
  assert(str);
  EnsureWriteable(len);                    // 确保可写的长度
  std::copy(str, str + len, BeginWrite()); // 将str放到写下标开始的方法
  HasWritten(len);                         // 移动写下标
}

void Buffer::Append(const void *data, size_t len)
{
  Append(static_cast<const char *>(data), len);
}

// 将buffer中的读下标的地方放到该buffer中的写下标位置
void Buffer::Append(const Buffer &buff)
{
  Append(buff.Peek(), buff.ReadableBytes());
}

/* 将fd的内容读到缓冲区，即writable的位置
这个函数通过 readv 函数实现了分散读取（scatter read），允许数据同时被写入到两个不同的内存区域，这在处理大量数据时可以提高效率。
同时，它还处理了可能发生的错误，并将读取的数据正确地存储到了 Buffer 对象的内部缓冲区中。*/
ssize_t Buffer::ReadFd(int fd, int *Errno)
{
  char buff[65535]; // 栈区 在栈上分配一个大小为 65535 字节的字符数组，用作临时缓冲区。
  struct iovec iov[2]; // 声明一个 iovec 结构体数组 iov，它包含两个元素。iovec 结构用于 readv 函数，可以指定多个内存区域进行读写。
  size_t writeable = WritableBytes(); // 先记录能写多少 调用成员函数 WritableBytes 来获取当前缓冲区可写入的字节数，并将其存储在变量 writeable 中。
  // 分散读，保证数据能全部读完
  //设置 iov 数组的第一个元素，iov_base 指向当前可写位置 BeginWrite，iov_len 设置为可写入的字节数 writeable。
  iov[0].iov_base = BeginWrite();
  iov[0].iov_len = writeable;
  //设置 iov 数组的第二个元素，iov_base 指向栈上分配的临时缓冲区 buff，iov_len 设置为 buff 的大小。
  iov[1].iov_base = buff;
  iov[1].iov_len = sizeof(buff);

  /*调用 readv 函数，从文件描述符 fd 读取数据到 iov 指定的两个内存区域。
  readv 会尝试从 fd 读取数据到 iov[0] 和 iov[1] 中，直到填满这两个区域或者 fd 中没有更多数据可读。len 存储读取的总字节数。
  这行代码调用了 readv 函数，从文件描述符 fd 读取数据，最多尝试填满 iov 数组中指定的两个内存区域。具体读取过程如下：
  readv 开始从 fd 读取数据。
  它首先尝试将数据读入 iov[0] 指定的内存区域，即 BeginWrite 处的缓冲区，直到该区域填满或者没有更多数据可读。
  如果 iov[0] 的区域被填满，或者还有更多数据需要读取，readv 会继续从 fd 读取数据，并将数据读入 iov[1] 指定的内存区域，即栈上分配的临时缓冲区 buff。
  readv 会一直读取，直到以下任一条件满足：
  所有 iov 指定的内存区域都被填满。
  fd 中没有更多数据可读。
  遇到了文件的 EOF（文件结束标志）。
  len 变量将存储实际读取的字节数。如果 len 小于 0，表示读取过程中发生了错误，Errno 指针将被用来记录错误码。

  使用 readv 的好处是可以减少数据复制的次数，提高读取大量数据时的效率。它允许系统一次系统调用就向多个目标缓冲区写入数据，而不是多次调用 read 函数并将数据复制到不同的位置。*/
  ssize_t len = readv(fd, iov, 2);

  if (len < 0)  //如果 readv 函数返回一个负值，表示读取失败。
  {
    *Errno = errno; //将当前的 errno（一个全局变量，存储了最近一次系统调用的错误码）值赋值给 Errno 指针指向的变量。
  }
  else if (static_cast<size_t>(len) <= writeable) //如果读取的字节数 len（转换为 size_t 类型）小于或等于可写入的字节数 writeable。
  {                   // 若len小于writable，说明写区可以容纳len
    writePos_ += len; // 将写指针 writePos_ 向前移动 len 字节，因为缓冲区的写区域已经填满了这么多数据。
  }
  else //否则，如果读取的字节数超过了缓冲区的可写入空间。
  {
    //将写指针移动到缓冲区的末尾，然后调用 Append 函数，将临时缓冲区 buff 中剩余的数据追加到 Buffer 对象的内部缓冲区 buffer_ 中。
    writePos_ = buffer_.size();                         // 写区写满了，下标移动到最后
    Append(buff, static_cast<size_t>(len - writeable)); // 剩余的长度
  }
  return len; //返回 readv 函数读取的总字节数。
}

// 将buffer中可读的区域写入fd中
ssize_t Buffer::WriteFd(int fd, int *Errno)
{
  //调用标准库函数 write，尝试将 Buffer 类中可读的数据写入到文件描述符 fd。
  //Peek 函数返回一个指向缓冲区当前读位置的指针，ReadableBytes 返回可读的字节数。
  //write 函数返回实际写入的字节数，存储在变量 len 中。
  ssize_t len = write(fd, Peek(), ReadableBytes());
  if (len < 0) //如果 write 函数返回的 len 小于 0，表示写入操作失败。
  {
    *Errno = errno;
    return len;
  }
  Retrieve(len); //调用 Buffer 类的 Retrieve 函数，将缓冲区中已写入的 len 字节的数据移除，更新读位置指针。
  return len;
}

char *Buffer::BeginPtr_()
{
  return &buffer_[0];
}

const char *Buffer::BeginPtr_() const
{
  return &buffer_[0];
}

// 扩展空间
void Buffer::MakeSpace_(size_t len)
{
  //检查当前缓冲区的可写空间（WritableBytes()）加上可前置空间（PrependableBytes()，即读指针之前的空间）是否小于 len。
  //如果是，说明当前空间不足以满足需求。
  if (WritableBytes() + PrependableBytes() < len)
  {
    /*如果空间不足，使用 resize 方法调整 buffer_ 的大小。
    新的大小是当前写位置 writePos_ 加上需要的长度 len 加 1，
    加 1 表示可能需要额外的空间来存储一个空字符（如果处理的是字符串），或者为其他目的预留空间。*/
    buffer_.resize(writePos_ + len + 1);
  }
  else //如果当前空间足够，执行 else 分支，开始移动缓冲区中的数据。
  {
    size_t readable = ReadableBytes(); //计算当前可读的字节数，即从读指针到写指针之间的字节数。
    //使用 std::copy 算法将从读指针到写指针的数据复制到缓冲区的开头。这是为了将所有未读的数据移动到缓冲区的起始位置，以便在它们之后写入新数据。
    std::copy(BeginPtr_() + readPos_, BeginPtr_() + writePos_, BeginPtr_());
    readPos_ = 0; //重置读指针到缓冲区的开始位置。
    writePos_ = readable; //将写指针设置为当前可读的字节数，因为所有数据已经被复制到了缓冲区的开始。
    assert(readable == ReadableBytes()); //使用断言来确保移动数据后，可读的字节数没有变化。这是一个调试检查，确保缓冲区的状态是一致的。
  }
}