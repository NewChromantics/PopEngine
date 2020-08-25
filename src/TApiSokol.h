#pragma once
#include "TBind.h"


namespace ApiSokol
{
    void    Bind(Bind::TContext& Context);

    class TSokolWrapper;
    DECLARE_BIND_TYPENAME(Sokol);
}

class SoySokol;

class ApiSokol::TSokolWrapper : public Bind::TObjectWrapper<BindType::Sokol, SoySokol>
{
public:
    TSokolWrapper(Bind::TContext& Context) :
        TObjectWrapper    ( Context )
    {
    }

    static void        CreateTemplate(Bind::TTemplate& Template);
    virtual void       Construct(Bind::TCallback& Params) override;

    // Initial Test
    void                    Test(Bind::TCallback& Params);

public:
    std::shared_ptr<SoySokol>&    mSoySokol = mObject;
};
