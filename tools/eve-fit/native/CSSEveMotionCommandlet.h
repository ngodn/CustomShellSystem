#pragma once
#include "Commandlets/Commandlet.h"
#include "CSSEveMotionCommandlet.generated.h"

UCLASS()
class UCSSEveMotionCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UCSSEveMotionCommandlet();
    virtual int32 Main(const FString& Params) override;
};
