#pragma once

#include "UIWidget.h"

class FStage;

class FDebugWidget : public IUIWidget
{
public:
    void BindStage(FStage *InStage);

    void Update(FGameContext &Context) override;
    void Render(FGameContext &Context) override;

private:
    FStage *Stage = nullptr;
    bool    bInvincible = false;
};
