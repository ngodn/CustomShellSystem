#pragma once
#include "Commandlets/Commandlet.h"
#include "CSSCopyAssetsCommandlet.generated.h"

// Isolated, explicit package migration using the pinned editor serializer.
UCLASS()
class UCSSCopyAssetsCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UCSSCopyAssetsCommandlet();
    virtual int32 Main(const FString& Params) override;
};
