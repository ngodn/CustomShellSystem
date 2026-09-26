#pragma once
#include "Commandlets/Commandlet.h"
#include "CSSEvePanelCommandlet.generated.h"

UCLASS()
class UCSSEvePanelCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UCSSEvePanelCommandlet();
    virtual int32 Main(const FString& Params) override;
};
