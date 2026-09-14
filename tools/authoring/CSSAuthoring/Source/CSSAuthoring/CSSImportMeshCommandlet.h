#pragma once

#include "Commandlets/Commandlet.h"
#include "CSSImportMeshCommandlet.generated.h"

/** Offline import of fitted geometry with an explicitly supplied reference skeleton. */
UCLASS()
class UCSSImportMeshCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UCSSImportMeshCommandlet();
    virtual int32 Main(const FString& Params) override;
};
