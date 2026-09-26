#pragma once
#include "Commandlets/Commandlet.h"
#include "CSSEveClothCommandlet.generated.h"

UCLASS()
class UCSSEveClothCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UCSSEveClothCommandlet();
    virtual int32 Main(const FString& Params) override;
};
