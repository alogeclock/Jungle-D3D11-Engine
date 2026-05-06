local DamagePostProcess = {}

function DamagePostProcess:Begin(params)
    params = params or {}

    self.elapsed = 0.0
    self.duration = params.duration or 0.7
    self.intensity = params.intensity or 1.0
end

function DamagePostProcess:Update(deltaTime, view)
    self.elapsed = self.elapsed + deltaTime

    local alpha = math.max(0.0, 1.0 - self.elapsed / self.duration)
    local intensity = self.intensity * alpha
    local chromaticAberration = 0.2 * alpha

    view.postProcess:SetScalar(
        "HitEffectIntensity",
        math.max(view.postProcess:GetScalar("HitEffectIntensity", 0.0), intensity)
    )

    view.postProcess:SetScalar(
        "ChromaticAberration",
        math.max(view.postProcess:GetScalar("ChromaticAberration", 0.0), chromaticAberration)
    )

    return self.elapsed >= self.duration
end

return DamagePostProcess
