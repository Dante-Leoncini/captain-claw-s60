// Minimal C++11 library pieces for C++03 compilers (RVCT 2.2 + STLport on S60v3).
// Pre-included in every C++ translation unit.
#ifndef CXX03_COMPAT_H_
#define CXX03_COMPAT_H_

#if defined(__cplusplus) && (__cplusplus < 201103L)

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <memory>
#include <utility>
#include <algorithm>

#define override
#define final

class nullptr_t_
{
public:
    template <class T> operator T*() const { return 0; }
    template <class C, class T> operator T C::*() const { return 0; }
private:
    void operator&() const;
};
static const nullptr_t_ nullptr = nullptr_t_();

namespace std {

//---------------------------------------------------------------------------
// shared_ptr / weak_ptr (single threaded)
//---------------------------------------------------------------------------

struct sp_counted_base_
{
    long use;
    long weak;

    sp_counted_base_() : use(1), weak(1) {}
    virtual ~sp_counted_base_() {}
    virtual void dispose() = 0;

    void add_ref() { ++use; }
    void release()
    {
        if (--use == 0)
        {
            dispose();
            weak_release();
        }
    }
    void weak_add_ref() { ++weak; }
    void weak_release()
    {
        if (--weak == 0)
            delete this;
    }
};

template <class Y>
struct sp_counted_impl_ : sp_counted_base_
{
    Y* p;
    explicit sp_counted_impl_(Y* ptr) : p(ptr) {}
    virtual void dispose() { delete p; }
};

template <class Y, class D>
struct sp_counted_impl_d_ : sp_counted_base_
{
    Y* p;
    D d;
    sp_counted_impl_d_(Y* ptr, D del) : p(ptr), d(del) {}
    virtual void dispose() { if (p) d(p); }
};

struct sp_nil_ {};
typedef int sp_nil_::* sp_null_t_;

struct sp_static_tag_ {};
struct sp_dynamic_tag_ {};
struct sp_const_tag_ {};
struct sp_reinterpret_tag_ {};

template <class T> class weak_ptr;

template <class T>
class shared_ptr
{
    typedef shared_ptr<T> this_type;
    typedef T* (this_type::*unspecified_bool_type)() const;

public:
    typedef T element_type;

    shared_ptr() : px(0), pn(0) {}
    shared_ptr(nullptr_t_) : px(0), pn(0) {}
    shared_ptr(sp_null_t_) : px(0), pn(0) {}

    template <class Y>
    explicit shared_ptr(Y* p) : px(p), pn(0)
    {
        if (p)
            pn = new sp_counted_impl_<Y>(p);
    }

    template <class Y, class D>
    shared_ptr(Y* p, D d) : px(p), pn(new sp_counted_impl_d_<Y, D>(p, d)) {}

    shared_ptr(const shared_ptr& r) : px(r.px), pn(r.pn)
    {
        if (pn)
            pn->add_ref();
    }

    template <class Y>
    shared_ptr(const shared_ptr<Y>& r) : px(r.px), pn(r.pn)
    {
        if (pn)
            pn->add_ref();
    }

    template <class Y>
    shared_ptr(const shared_ptr<Y>& r, T* p) : px(p), pn(r.pn)
    {
        if (pn)
            pn->add_ref();
    }

    template <class Y>
    explicit shared_ptr(const weak_ptr<Y>& r) : px(0), pn(0)
    {
        if (r.pn && r.pn->use > 0)
        {
            px = r.px;
            pn = r.pn;
            pn->add_ref();
        }
    }

    template <class Y>
    shared_ptr(const shared_ptr<Y>& r, sp_static_tag_) : px(static_cast<T*>(r.px)), pn(r.pn)
    {
        if (pn)
            pn->add_ref();
    }

    template <class Y>
    shared_ptr(const shared_ptr<Y>& r, sp_const_tag_) : px(const_cast<T*>(r.px)), pn(r.pn)
    {
        if (pn)
            pn->add_ref();
    }

    template <class Y>
    shared_ptr(const shared_ptr<Y>& r, sp_reinterpret_tag_) : px(reinterpret_cast<T*>(r.px)), pn(r.pn)
    {
        if (pn)
            pn->add_ref();
    }

    template <class Y>
    shared_ptr(const shared_ptr<Y>& r, sp_dynamic_tag_) : px(dynamic_cast<T*>(r.px)), pn(0)
    {
        if (px)
        {
            pn = r.pn;
            if (pn)
                pn->add_ref();
        }
    }

    ~shared_ptr()
    {
        if (pn)
            pn->release();
    }

    shared_ptr& operator=(const shared_ptr& r)
    {
        this_type(r).swap(*this);
        return *this;
    }

    template <class Y>
    shared_ptr& operator=(const shared_ptr<Y>& r)
    {
        this_type(r).swap(*this);
        return *this;
    }

    shared_ptr& operator=(nullptr_t_)
    {
        reset();
        return *this;
    }

    void reset() { this_type().swap(*this); }

    template <class Y>
    void reset(Y* p) { this_type(p).swap(*this); }

    template <class Y, class D>
    void reset(Y* p, D d) { this_type(p, d).swap(*this); }

    T* get() const { return px; }
    T& operator*() const { return *px; }
    T* operator->() const { return px; }

    long use_count() const { return pn ? pn->use : 0; }
    bool unique() const { return use_count() == 1; }

    operator unspecified_bool_type() const { return px == 0 ? 0 : &this_type::get; }
    bool operator!() const { return px == 0; }

    void swap(shared_ptr& other)
    {
        std::swap(px, other.px);
        std::swap(pn, other.pn);
    }

    T* px;
    sp_counted_base_* pn;
};

template <class T, class U>
inline bool operator==(const shared_ptr<T>& a, const shared_ptr<U>& b) { return a.get() == b.get(); }
template <class T, class U>
inline bool operator!=(const shared_ptr<T>& a, const shared_ptr<U>& b) { return a.get() != b.get(); }
template <class T, class U>
inline bool operator<(const shared_ptr<T>& a, const shared_ptr<U>& b) { return a.pn < b.pn; }

template <class T> inline bool operator==(const shared_ptr<T>& a, nullptr_t_) { return a.get() == 0; }
template <class T> inline bool operator==(nullptr_t_, const shared_ptr<T>& a) { return a.get() == 0; }
template <class T> inline bool operator!=(const shared_ptr<T>& a, nullptr_t_) { return a.get() != 0; }
template <class T> inline bool operator!=(nullptr_t_, const shared_ptr<T>& a) { return a.get() != 0; }
template <class T> inline bool operator==(const shared_ptr<T>& a, sp_null_t_) { return a.get() == 0; }
template <class T> inline bool operator!=(const shared_ptr<T>& a, sp_null_t_) { return a.get() != 0; }

template <class T, class U>
inline shared_ptr<T> static_pointer_cast(const shared_ptr<U>& r) { return shared_ptr<T>(r, sp_static_tag_()); }
template <class T, class U>
inline shared_ptr<T> dynamic_pointer_cast(const shared_ptr<U>& r) { return shared_ptr<T>(r, sp_dynamic_tag_()); }
template <class T, class U>
inline shared_ptr<T> const_pointer_cast(const shared_ptr<U>& r) { return shared_ptr<T>(r, sp_const_tag_()); }
template <class T, class U>
inline shared_ptr<T> reinterpret_pointer_cast(const shared_ptr<U>& r) { return shared_ptr<T>(r, sp_reinterpret_tag_()); }

template <class T>
class weak_ptr
{
public:
    typedef T element_type;

    weak_ptr() : px(0), pn(0) {}

    weak_ptr(const weak_ptr& r) : px(r.px), pn(r.pn)
    {
        if (pn)
            pn->weak_add_ref();
    }

    template <class Y>
    weak_ptr(const weak_ptr<Y>& r) : px(r.px), pn(r.pn)
    {
        if (pn)
            pn->weak_add_ref();
    }

    template <class Y>
    weak_ptr(const shared_ptr<Y>& r) : px(r.px), pn(r.pn)
    {
        if (pn)
            pn->weak_add_ref();
    }

    ~weak_ptr()
    {
        if (pn)
            pn->weak_release();
    }

    weak_ptr& operator=(const weak_ptr& r)
    {
        weak_ptr(r).swap(*this);
        return *this;
    }

    template <class Y>
    weak_ptr& operator=(const weak_ptr<Y>& r)
    {
        weak_ptr(r).swap(*this);
        return *this;
    }

    template <class Y>
    weak_ptr& operator=(const shared_ptr<Y>& r)
    {
        weak_ptr(r).swap(*this);
        return *this;
    }

    void reset() { weak_ptr().swap(*this); }

    long use_count() const { return pn ? pn->use : 0; }
    bool expired() const { return use_count() == 0; }
    shared_ptr<T> lock() const { return shared_ptr<T>(*this); }

    void swap(weak_ptr& other)
    {
        std::swap(px, other.px);
        std::swap(pn, other.pn);
    }

    T* px;
    sp_counted_base_* pn;
};

template <class T, class U>
inline bool operator<(const weak_ptr<T>& a, const weak_ptr<U>& b) { return a.pn < b.pn; }

template <class T>
inline shared_ptr<T> make_shared() { return shared_ptr<T>(new T()); }
template <class T, class A1>
inline shared_ptr<T> make_shared(const A1& a1) { return shared_ptr<T>(new T(a1)); }
template <class T, class A1, class A2>
inline shared_ptr<T> make_shared(const A1& a1, const A2& a2) { return shared_ptr<T>(new T(a1, a2)); }
template <class T, class A1, class A2, class A3>
inline shared_ptr<T> make_shared(const A1& a1, const A2& a2, const A3& a3) { return shared_ptr<T>(new T(a1, a2, a3)); }
template <class T, class A1, class A2, class A3, class A4>
inline shared_ptr<T> make_shared(const A1& a1, const A2& a2, const A3& a3, const A4& a4)
{
    return shared_ptr<T>(new T(a1, a2, a3, a4));
}
template <class T, class A1, class A2, class A3, class A4, class A5>
inline shared_ptr<T> make_shared(const A1& a1, const A2& a2, const A3& a3, const A4& a4, const A5& a5)
{
    return shared_ptr<T>(new T(a1, a2, a3, a4, a5));
}

//---------------------------------------------------------------------------
// unique_ptr (ownership moves on copy, like auto_ptr)
//---------------------------------------------------------------------------

template <class T>
struct default_delete
{
    void operator()(T* p) const { delete p; }
};

template <class T>
struct default_delete<T[]>
{
    void operator()(T* p) const { delete[] p; }
};

template <class T>
struct up_element_ { typedef T type; };
template <class T>
struct up_element_<T[]> { typedef T type; };

template <class T, class D = default_delete<T> >
class unique_ptr
{
    typedef typename up_element_<T>::type E;
    typedef unique_ptr<T, D> this_type;
    typedef E* (this_type::*unspecified_bool_type)() const;

public:
    unique_ptr() : p_(0), d_() {}
    unique_ptr(nullptr_t_) : p_(0), d_() {}
    explicit unique_ptr(E* p) : p_(p), d_() {}
    unique_ptr(E* p, D d) : p_(p), d_(d) {}
    unique_ptr(const unique_ptr& r) : p_(r.release()), d_(r.d_) {}
    ~unique_ptr() { if (p_) d_(p_); }

    unique_ptr& operator=(const unique_ptr& r)
    {
        if (&r != this)
            reset(r.release());
        return *this;
    }

    unique_ptr& operator=(nullptr_t_)
    {
        reset();
        return *this;
    }

    E* get() const { return p_; }
    E& operator*() const { return *p_; }
    E* operator->() const { return p_; }
    E& operator[](size_t i) const { return p_[i]; }

    E* release() const
    {
        E* p = p_;
        p_ = 0;
        return p;
    }

    void reset(E* p = 0)
    {
        E* old = p_;
        p_ = p;
        if (old && old != p)
            d_(old);
    }

    operator unspecified_bool_type() const { return p_ == 0 ? 0 : &this_type::get; }
    bool operator!() const { return p_ == 0; }

private:
    mutable E* p_;
    D d_;
};

template <class T, class D> inline bool operator==(const unique_ptr<T, D>& a, nullptr_t_) { return !a; }
template <class T, class D> inline bool operator!=(const unique_ptr<T, D>& a, nullptr_t_) { return !!a; }

template <class T>
inline const T& move(const T& t) { return t; }

//---------------------------------------------------------------------------
// string conversions
//---------------------------------------------------------------------------

inline string to_string(int v) { char b[16]; sprintf(b, "%d", v); return string(b); }
inline string to_string(unsigned v) { char b[16]; sprintf(b, "%u", v); return string(b); }
inline string to_string(long v) { char b[24]; sprintf(b, "%ld", v); return string(b); }
inline string to_string(unsigned long v) { char b[24]; sprintf(b, "%lu", v); return string(b); }
inline string to_string(double v) { char b[64]; sprintf(b, "%f", v); return string(b); }
inline string to_string(float v) { return to_string((double)v); }

inline int stoi(const string& s, size_t* idx = 0, int base = 10)
{
    char* end;
    long v = strtol(s.c_str(), &end, base);
    if (idx)
        *idx = end - s.c_str();
    return (int)v;
}
inline long stol(const string& s, size_t* idx = 0, int base = 10)
{
    char* end;
    long v = strtol(s.c_str(), &end, base);
    if (idx)
        *idx = end - s.c_str();
    return v;
}
inline unsigned long stoul(const string& s, size_t* idx = 0, int base = 10)
{
    char* end;
    unsigned long v = strtoul(s.c_str(), &end, base);
    if (idx)
        *idx = end - s.c_str();
    return v;
}
inline double stod(const string& s, size_t* idx = 0)
{
    char* end;
    double v = strtod(s.c_str(), &end);
    if (idx)
        *idx = end - s.c_str();
    return v;
}
inline float stof(const string& s, size_t* idx = 0) { return (float)stod(s, idx); }

} // namespace std

#endif // C++03

#endif
