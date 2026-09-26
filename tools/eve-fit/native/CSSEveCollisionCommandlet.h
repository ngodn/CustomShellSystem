#pragma once
#include "Commandlets/Commandlet.h"
#include "CSSEveCollisionCommandlet.generated.h"

UCLASS()
class UCSSEveCollisionCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UCSSEveCollisionCommandlet();
    virtual int32 Main(const FString& Params) override;
};
