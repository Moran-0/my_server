#pragma once
#include <cassert>
#include <stdexcept>
#define OS_LINUX

// 更安全的基类：禁用拷贝
class NonCopyable
{
  protected:
    NonCopyable() = default;
    ~NonCopyable() = default;
    NonCopyable(const NonCopyable&) = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;
};

// 更安全的基类：禁用移动
class NonMovable
{
  protected:
    NonMovable() = default;
    ~NonMovable() = default;
    NonMovable(NonMovable&&) = delete;
    NonMovable& operator=(NonMovable&&) = delete;
};

// 组合基类：禁用拷贝和移动
class NonCopyableAndMovable : public NonCopyable, public NonMovable
{
  protected:
    NonCopyableAndMovable() = default;
    ~NonCopyableAndMovable() = default;
};