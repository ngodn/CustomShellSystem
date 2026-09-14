#pragma once

#include "Commandlets/Commandlet.h"
#include "CSSCookAssetsCommandlet.generated.h"

/** Experimental asset-only Windows cook using the installed Linux editor. */
UCLASS()
class UCSSCookAssetsCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UCSSCookAssetsCommandlet();
    virtual int32 Main(const FString& Params) override;
};
