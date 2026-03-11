#pragma once

enum class EUIPopupAction
{
    None,

    ClosePopup,
    GoToNextScoreboardPage,

    RetryCurrentStage,
    GoToTitleScene,
    GoToNextStage,

    OpenSaveScorePopup,
    ConfirmSaveScore,
    CancelSaveScore
};