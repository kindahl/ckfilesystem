#pragma once
#include <ckcore/types.hh>
#include <ckcore/progress.hh>

class Progress : public ckcore::Progress
{
private:
    unsigned char last_progress_;

public:
    Progress();

    // ckcore::Progress interface.
    void SetProgress(unsigned char progress);
    void SetStatus(const ckcore::tchar *format,...);
    void Notify(MessageType type,const ckcore::tchar *format,...);
    bool Cancelled();
};

