#include <setjmp.h>
#include <unwind.h>
#include <stdfil.h>
#include <vector>
#include <string>

using namespace std;

static vector<string> things;

struct C {
    C()
    {
        things.push_back("C::C");
    }

    ~C()
    {
        things.push_back("C::~C");
    }
};

struct D {
    D()
    {
        things.push_back("D::D");
    }

    ~D()
    {
        things.push_back("D::~D");
    }
};

static jmp_buf jmpBuf;

static _Unwind_Reason_Code stopCallback(
    int version, _Unwind_Action actions, _Unwind_Exception_Class exceptionClass,
    _Unwind_Exception* exceptionObject, _Unwind_Context* Context, void* stopArg)
{
    ZASSERT(stopArg == (void*)666);
    if (actions & _UA_END_OF_STACK)
        longjmp(jmpBuf, 1);
    return _URC_NO_REASON;
}

static __attribute__((__noinline__)) void foo()
{
    C c;
    _Unwind_Exception* exceptionObject = new _Unwind_Exception;
    exceptionObject->exception_class = 0;
    exceptionObject->exception_cleanup = nullptr;
    _Unwind_ForcedUnwind(exceptionObject, stopCallback, (void*)666);
}

static __attribute__((__noinline__)) void bar()
{
    D d;
    foo();
}

static unsigned counter;

class Exception {
public:
    Exception(int x)
        : m_x(x)
    {
    }

    int x() const { return m_x; }

private:
    int m_x;
};

class RAII {
public:
    RAII()
    {
        ZASSERT(++counter == 1);
    }

    ~RAII()
    {
        ZASSERT(++counter == 3);
    }
};

void stuff()
{
    throw Exception(++counter);
}

void thingy()
{
    RAII raii;
    stuff();
}

void doit()
{
    try {
        thingy();
        ZASSERT(!"Should not get here");
    } catch (Exception e) {
        ZASSERT(e.x() == 2);
    } catch (...) {
        ZASSERT(!"Should not get here");
    }
    ZASSERT(counter == 3);
}

int main()
{
    for (unsigned index = 10000; index--;) {
        things.clear();
        if (!setjmp(jmpBuf))
            bar();
        ZASSERT(things.size() == 4);
        ZASSERT(things[0] == "D::D");
        ZASSERT(things[1] == "C::C");
        ZASSERT(things[2] == "C::~C");
        ZASSERT(things[3] == "D::~D");

        counter = 0;
        doit();
    }
    return 0;
}
