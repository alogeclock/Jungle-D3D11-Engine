local vehicle = nil
local movement = nil

function BeginPlay()
    vehicle = obj:AsVehiclePawn()
    if vehicle == nil then
        vehicle = obj:AsCarPawn()
    end

    if vehicle ~= nil then
        movement = vehicle:GetVehicleMovement()
    else
        movement = obj:GetVehicleMovement()
    end
end

function EndPlay()
    if movement ~= nil then
        movement:StopImmediately()
    end
end

function Tick(dt)
    if movement == nil then
        return
    end

    local throttle = 0
    if Input.GetKey(Key.W) then throttle = throttle + 1 end
    if Input.GetKey(Key.S) then throttle = throttle - 1 end

    local steering = 0
    if Input.GetKey(Key.A) then steering = steering - 1 end
    if Input.GetKey(Key.D) then steering = steering + 1 end

    movement:SetThrottleInput(throttle)
    movement:SetSteeringInput(steering)
    movement:SetHandbrakeInput(Input.GetKey(Key.Space))

    if Input.GetKeyDown(Key.R) then
        movement:StopImmediately()
    end
end
