#pragma once

#include "Commandlets/Commandlet.h"
#include "CSSBindClothCommandlet.generated.h"

/** Build native cloth on authored skirt sections without changing the rig. */
UCLASS()
class UCSSBindClothCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UCSSBindClothCommandlet();
    virtual int32 Main(const FString& Params) override;
};
