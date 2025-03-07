#include <iostream>
#include <exception>
#include <string>
#include "unwind.h"
using namespace std;
class Exception : public exception {
public:
    Exception(const string& str) : m_str(str) { }
    virtual ~Exception() throw() { }
    virtual const char* what() const throw() { return m_str.c_str(); }
private:
    string m_str;
};
struct RAII {
    RAII() { cout << "Every time I turn around." << endl ; }
    ~RAII() { cout << "I start to hear a familiar sound." << endl; }
};
void foo() { throw Exception("Counting down as time stands still."); }
void bar() {
    RAII raii;
    foo();
}
int main(int argc, char** argv) {
    try {
        bar();
        cout << "Fever dream high in the quiet of the night." << endl;
    } catch (exception& e) {
        cout << "Caught exception: what = " << e.what() << endl;
    } catch (...) {
        cout << "What doesn't kill me makes me want you more." << endl;
    }
    cout << "Yes I think it's time to kill." << endl;
    return 0;
}
extern "C" _Unwind_Reason_Code __gxx_personality_v0(
    int version, _Unwind_Action actions, uint64_t exceptionClass,
    _Unwind_Exception* unwind_exception, _Unwind_Context* context)
{
    static unsigned count;
    if (actions & _UA_SEARCH_PHASE)
        return _URC_HANDLER_FOUND;
    if (++count == 2)
        return _URC_INSTALL_CONTEXT;
    return _URC_CONTINUE_UNWIND;
}
