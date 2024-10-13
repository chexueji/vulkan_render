#ifndef NONCOPYABLE_H
#define NONCOPYABLE_H

namespace VR {
/** protocol class. used to delete assignment operator. */
namespace backend {

class NonCopyable {
public:
    NonCopyable()                    = default;
    NonCopyable(const NonCopyable&)  = delete;
    NonCopyable(const NonCopyable&&) = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;
    NonCopyable& operator=(const NonCopyable&&) = delete;
};
} // namespace backend
} // namespace VR

#endif // NONCOPYABLE_H
