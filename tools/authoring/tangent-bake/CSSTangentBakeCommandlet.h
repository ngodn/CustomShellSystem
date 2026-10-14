#pragma once
#include "Commandlets/Commandlet.h"
#include "CSSTangentBakeCommandlet.generated.h"

UCLASS()
class UCSSTangentBakeCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UCSSTangentBakeCommandlet();
    virtual int32 Main(const FString& Params) override;
};
