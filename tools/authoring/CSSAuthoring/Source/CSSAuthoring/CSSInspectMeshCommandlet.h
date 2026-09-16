#pragma once

#include "Commandlets/Commandlet.h"
#include "CSSInspectMeshCommandlet.generated.h"

/** Reads a saved skeletal mesh back and writes what it actually contains as JSON. */
UCLASS()
class UCSSInspectMeshCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UCSSInspectMeshCommandlet();
    virtual int32 Main(const FString& Params) override;
};
