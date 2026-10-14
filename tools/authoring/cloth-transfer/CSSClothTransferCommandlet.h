#pragma once
#include "Commandlets/Commandlet.h"
#include "CSSClothTransferCommandlet.generated.h"

UCLASS()
class UCSSClothTransferCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UCSSClothTransferCommandlet();
    virtual int32 Main(const FString& Params) override;
};
