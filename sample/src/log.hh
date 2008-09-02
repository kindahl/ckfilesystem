#pragma once
#include <ckcore/types.hh>
#include <ckcore/log.hh>

class Log : public ckcore::Log
{
public:
    // ckcore::Log interface.
    void Print(const ckcore::tchar *format,...);
    void PrintLine(const ckcore::tchar *format,...);
};

