/**
 * Copyright 2020 Kaloian Manassiev
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
 * associated documentation files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge, publish, distribute,
 * sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
 * NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#pragma once

#include <boost/assert.hpp>
#include <boost/format.hpp>
#include <exception>
#include <string>

namespace ruralpi {

class Exception : public std::exception {
public:
    Exception(std::string message);
    Exception(boost::format formatter);

    const char *what() const noexcept override;

private:
    std::string _message;
};

class SystemException : public Exception {
public:
    using Exception::Exception;

    /**
     * These `throwXXX` methods may only be called after a system call has returned an error (< 0)
     * return code. They will take the `errno` from the syscall and produce a SystemException with
     * an appropriate message.
     */
    static void throwFromErrno(const std::string &context);
    static void throwFromErrno(const boost::format &context);
    static void throwFromErrno();

    /**
     * Returns a formatted last system error information from `errno`.
     */
    static std::string getLastError();
};

#define SYSCALL_MSG(x, msg)                                                                        \
    [&] {                                                                                          \
        int res = x;                                                                               \
        if (res < 0)                                                                               \
            SystemException::throwFromErrno(msg);                                                  \
        return res;                                                                                \
    }()

#define SYSCALL(x) SYSCALL_MSG(x, #x)

class ScopedGuard {
public:
    ScopedGuard(std::function<void()> fn) : _fn(std::move(fn)) {}
    ~ScopedGuard();

    ScopedGuard(ScopedGuard &&);

private:
    std::function<void()> _fn;
};

} // namespace ruralpi
