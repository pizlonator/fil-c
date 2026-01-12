#include <stdfil.h>
#include <string>

struct Where {
    Where() = default;

    Where(const std::string& function, const std::string& filename, int lineNumber)
        : function(function)
        , filename(filename)
        , lineNumber(lineNumber)
    {
    }

    bool operator==(const Where& other) const
    {
        return function == other.function
            && filename == other.filename
            && lineNumber == other.lineNumber;
    }
    
    std::string function;
    std::string filename;
    int lineNumber { 0 };
};

struct GetWhereData {
    Where where;
    unsigned count;
};

bool getWhereCallback(const zstack_frame_description* description, void* arg)
{
    GetWhereData* data = static_cast<GetWhereData*>(arg);
    if (data->count++ < 1)
        return true;
    zprintf("Where: %s, %s, %d\n",
            description->function_name, description->filename, description->line);
    data->where.function = description->function_name;
    data->where.filename = description->filename;
    data->where.lineNumber = description->line;
    return false;
}

Where getWhere()
{
    GetWhereData data;
    zstack_scan(getWhereCallback, &data);
    return data.where;
}

template<typename T>
struct Foo {
    template<typename U>
    void bar()
    {
        ZASSERT(getWhere() == Where("void Foo<int>::bar<double>()", "filc/tests/debuginfo/test.cpp", 56));
    }

    template<typename U>
    void baz()
    {
#include "test_include2.inc"
    }
};

int main()
{
    ZASSERT(getWhere() == Where("main", "filc/tests/debuginfo/test.cpp", 68));

    Foo<int>().bar<double>();

#include "test_include.inc"

    Foo<short>().baz<char>();

    return 0;
}

