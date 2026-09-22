# PhysX Vehicle Implementation Plan

## Reference

- NVIDIA PhysX 4.1 Vehicles: <https://nvidiagameworks.github.io/PhysX/4.1/documentation/physxguide/Manual/Vehicles.html#vehicle-creation>
- PhysX Vehicle headers: `KraftonEngine/ThirdParty/PhysX/Include/physx/vehicle/`
- PhysX Vehicle lib: `KraftonEngine/ThirdParty/PhysX/Lib/x64/{Debug,Release}/PhysXVehicle_static_64.lib`

## Current State

현재 엔진에는 Vehicle용 데이터 타입만 있고 실제 runtime은 없다.

- 존재하는 타입
  - `KraftonEngine/Source/Engine/Physics/Systems/Vehicle/VehicleTypes.h`
  - `KraftonEngine/Source/Engine/Physics/Systems/Vehicle/VehicleWheelTypes.h`
  - `KraftonEngine/Source/Engine/Physics/Systems/Vehicle/VehicleInputTypes.h`
  - `KraftonEngine/Source/Engine/Physics/Systems/Vehicle/VehicleDebugTypes.h`
- 없는 것
  - `PxVehicleDrive4W` 생성/소유 코드
  - suspension raycast / `PxVehicleUpdates` 호출 코드
  - `UVehicleMovementComponent`
  - `AVehiclePawn`
  - vehicle pawn용 GameMode
- PhysXVehicle 정적 라이브러리는 존재하지만 빌드 링크 목록에는 아직 없다.
  - `Scripts/GenerateProjectFiles.py`의 `PHYSX_LIBS`
  - `KraftonEngine/KraftonEngine.vcxproj`의 `AdditionalDependencies`

현재 PIE pawn 빙의는 `AGameModeBase::StartMatch()`에서 진행된다.

1. `UWorld::BeginPlay()` 이후 `GameMode->StartMatch()` 호출
2. `APlayerController` spawn
3. scene 안에서 `bAutoPossessPlayer == true`인 첫 `APawn`을 찾음
4. 있으면 그 pawn을 possess
5. 없으면 `DefaultPawnClass`를 spawn해서 possess

따라서 vehicle을 player pawn으로 쓰는 방법은 두 가지다.

- scene에 배치된 `AVehiclePawn` 하나만 `bAutoPossessPlayer=true`로 둔다.
- `AGameModeVehicle`을 만들고 `DefaultPawnClass = AVehiclePawn::StaticClass()`로 설정한다.

## Goal

1차 목표는 4륜 `PxVehicleDrive4W` 기반 vehicle pawn을 PIE에서 직접 조종할 수 있게 만드는 것이다.

성공 기준:

- PIE 시작 시 `AVehiclePawn`이 possess된다.
- WASD 또는 기존 입력 시스템으로 throttle, brake, steering, handbrake가 들어간다.
- PhysX suspension raycast가 바닥을 감지한다.
- `PxVehicleUpdates` 결과로 chassis가 움직인다.
- wheel visual transform이 steering, rotation, suspension pose를 따라간다.
- 소유되지 않은 pawn/script는 입력을 소비하지 않는다.

## Architecture

권장 구조:

```text
AVehiclePawn
  - UStaticMeshComponent or UPrimitiveComponent: chassis/root visual
  - UStaticMeshComponent[4]: wheel visuals
  - UCameraComponent
  - UVehicleMovementComponent

UVehicleMovementComponent
  - editable tuning data
  - FVehicleBuildDesc 생성
  - input state 저장
  - physics scene에 vehicle create/destroy/input 전달

FPhysXPhysicsScene
  - FPhysXVehicleInstance 목록 소유
  - Vehicle SDK init/shutdown
  - suspension raycast
  - PxVehicleUpdates
  - chassis/wheel sync

FPhysXVehicleInstance
  - PxRigidDynamic* ChassisActor
  - PxVehicleDrive4W* Vehicle
  - PxVehicleWheelsSimData setup 결과
  - PxBatchQuery and query buffers
  - PxVehicleWheelQueryResult buffers
  - PxVehicleDrivableSurfaceToTireFrictionPairs*
  - FVehicleInputState
```

처음에는 PhysX backend 전용으로 구현한다. 이후 다른 physics backend가 필요해지면 `IPhysicsSceneInterface`에 vehicle API를 정식으로 추가한다.

## Step 1. Build Integration

`PhysXVehicle_static_64.lib`를 빌드 링크에 추가한다.

- `Scripts/GenerateProjectFiles.py`
  - `PHYSX_LIBS`에 `PhysXVehicle_static_64.lib` 추가
- `KraftonEngine/KraftonEngine.vcxproj`
  - Debug/Release/Game/Demo의 PhysX dependency 목록에 동일하게 추가
- 이후 `GenerateProjectFiles.bat` 또는 `python Scripts/GenerateProjectFiles.py`로 프로젝트 파일 재생성

주의:

- 수동으로 `.vcxproj`만 고치면 project generation 때 다시 빠질 수 있으므로 `GenerateProjectFiles.py`가 source of truth다.

## Step 2. Vehicle SDK Init / Shutdown

`FPhysXPhysicsScene`의 shared PhysX 생명주기에 Vehicle SDK 초기화와 종료를 붙인다.

초기화 순서:

```cpp
PxCreateFoundation(...);
PxCreatePhysics(...);
PxInitVehicleSDK(*Physics);
PxVehicleSetBasisVectors(PxVec3(0, 0, 1), PxVec3(1, 0, 0));
PxVehicleSetUpdateMode(PxVehicleUpdateMode::eVELOCITY_CHANGE);
```

종료 순서:

```cpp
PxCloseVehicleSDK();
Physics->release();
Foundation->release();
```

엔진 좌표계 기준:

- gravity가 `Z = -9.81`이므로 up vector는 `+Z`
- actor forward가 보통 `+X`이면 vehicle forward는 `+X`
- 실제 mesh forward가 다르면 `AVehiclePawn` visual transform 쪽에서 보정하고 PhysX 기준은 고정한다.

## Step 3. Vehicle Creation

PhysX 문서의 vehicle creation 흐름을 그대로 따른다.

1. `PxVehicleWheelsSimData::allocate(4)`
2. wheel data 설정
   - radius
   - width
   - mass
   - moment of inertia
   - max steer
   - handbrake torque
3. tire data 설정
   - tire type
4. suspension data 설정
   - max compression
   - max droop
   - spring strength
   - damper rate
   - sprung mass
5. `PxVehicleComputeSprungMasses`로 각 wheel의 sprung mass 계산
6. wheel center offset 설정
7. suspension travel direction 설정
8. wheel shape mapping 설정
9. `PxVehicleDriveSimData4W` 설정
   - differential
   - engine torque curve
   - gears
   - clutch
   - Ackermann geometry
10. chassis `PxRigidDynamic` 생성
11. chassis collision shape와 optional wheel collision shape 추가
12. `PxVehicleDrive4W::allocate(4)`
13. `PxVehicleDrive4W::setup(...)`
14. `Vehicle->setToRestState()`

`setToRestState()` must be called before the chassis actor is added to the scene.

Correct order:

```cpp
Vehicle->setup(Physics, ChassisActor, WheelsSimData, DriveSimData, 0);
Vehicle->setToRestState();
Scene->addActor(*ChassisActor);
```

Do not call `setToRestState()` after `Scene->addActor(...)`.

After `setToRestState()`, run `PxVehicleSuspensionRaycasts` at least once before the first `PxVehicleUpdates`, because the call invalidates cached suspension hit planes.
15. actor를 scene에 add

1차 구현에서는 wheel collision shape를 생략하고 `setWheelShapeMapping(i, -1)`로 시작할 수 있다. 이 경우 wheel pose는 직접 query result와 wheel dynamic data를 읽어 visual component에 반영한다. 충돌이 더 필요해지면 wheel shape를 추가한다.

## Step 4. Drivable Surface and Filtering

Vehicle suspension raycast는 일반 raycast와 목적이 다르다.

필요한 필터링:

- 자기 vehicle actor는 suspension raycast에서 제외
- static floor, road mesh는 drivable surface로 포함
- trigger volume, projectile, pawn 등은 기본적으로 제외

초기 정책:

- `ECC_WorldStatic`은 drivable
- `ECC_WorldDynamic`은 옵션으로 drivable
- vehicle chassis/wheel shape는 non-drivable
- owner UUID 또는 PhysX filter word를 사용해 self-hit 제외

tire friction:

- 1차 구현은 default material 하나와 tire type 하나만 사용
- 이후 surface material별 friction 확장
  - asphalt
  - dirt
  - ice
  - grass

## Step 5. Simulation Order

`FPhysXPhysicsScene::Simulate()` 내부에서 PhysX scene simulate 전에 vehicle update를 넣는다.

권장 순서:

```text
FPhysXPhysicsScene::Simulate(DeltaTime)
  1. 일반 body pre-sync
     - 단, vehicle-owned actor는 제외
  2. Vehicle suspension raycasts
  3. Vehicle input smoothing
  4. PxVehicleUpdates
  5. Scene->simulate(DeltaTime)

FPhysXPhysicsScene::FetchResults()
  1. Scene->fetchResults(...)
  2. 일반 dynamic body transform sync
     - 단, vehicle-owned actor는 별도 처리
  3. vehicle chassis transform sync
  4. wheel visual transform sync
  5. contact/trigger event dispatch
```

중요:

현재 일반 dynamic body는 simulate 전에 엔진 transform을 PhysX actor로 다시 밀어 넣는 pre-sync가 있다. vehicle chassis가 이 경로에 같이 들어가면 `PxVehicleUpdates`가 만든 결과를 엔진 transform이 덮어쓸 수 있다.

따라서 vehicle actor는 다음 중 하나로 처리해야 한다.

- 일반 `BodyMappings`에 넣지 않고 vehicle runtime이 별도 소유
- 또는 `FPhysicsBodyInstance`에 `bVehicleOwned` 같은 플래그를 두고 pre-sync/sync 경로에서 제외

1차 구현은 별도 소유가 더 단순하다.

Wheel visual sync reads the same `PxVehicleWheelQueryResult` buffer that was passed to `PxVehicleUpdates`.

- `FPhysXVehicleInstance` owns `PxVehicleWheelQueryResult` and the backing `PxWheelQueryResult` array as persistent members.
- The pointer passed in `Simulate()` and the pointer read in `FetchResults()` must be identical.
- Do not allocate this buffer on the stack or as a frame-local temporary.
- Minimum lifetime is vehicle creation to vehicle destruction.
- Per frame, the buffer must remain valid from `Simulate()` start through `FetchResults()` wheel visual sync completion.

## Step 6. Engine-facing Component

`UVehicleMovementComponent`를 추가한다.

역할:

- editor-exposed tuning data 보관
- `FVehicleBuildDesc` 구성
- BeginPlay에서 physics scene에 vehicle 생성 요청
- EndPlay 또는 destroy 시 vehicle 제거
- input state 업데이트
- speed, rpm, gear, grounded state 같은 debug getter 제공

초기 public API:

```cpp
void SetThrottle(float Value);
void SetBrake(float Value);
void SetSteering(float Value);
void SetHandbrake(bool bEnabled);

float GetForwardSpeedKmh() const;
int32 GetCurrentGear() const;
bool IsInAir() const;
```

Tick에서는 actor transform을 직접 이동시키지 않는다. 이동은 PhysX vehicle update와 rigid body simulation 결과만 신뢰한다.

## Step 7. AVehiclePawn

`AVehiclePawn`을 추가한다.

기본 구성:

- root/chassis primitive component
- chassis mesh visual
- 4 wheel mesh components
- camera component
- `UVehicleMovementComponent`

입력:

- C++에서 직접 처리하거나 Lua script에서 movement component API 호출
- Lua script를 쓴다면 반드시 possessed check가 필요하다.

예상 Lua 흐름:

```lua
if not obj:IsPossessed() then
    return
end

local move = obj:GetVehicleMovement()
move:SetThrottle(Input:IsKeyDown("W") and 1.0 or 0.0)
move:SetBrake(Input:IsKeyDown("S") and 1.0 or 0.0)
move:SetSteering((Input:IsKeyDown("D") and 1.0 or 0.0) - (Input:IsKeyDown("A") and 1.0 or 0.0))
move:SetHandbrake(Input:IsKeyDown("Space"))
```

현재 character/car script가 전역 `Input`을 직접 poll하는 구조라, possessed guard가 없으면 여러 pawn이 동시에 움직일 수 있다.

## Step 8. PIE Possession

옵션 A: scene 배치 vehicle possess

1. scene에 `AVehiclePawn` 배치
2. 기존 `ACharacter`의 `bAutoPossessPlayer=false`
3. `AVehiclePawn`의 `bAutoPossessPlayer=true`
4. PIE 시작
5. `AGameModeBase::FindAutoPossessPawn()`이 vehicle을 찾아 possess

옵션 B: GameMode default pawn 교체

1. `AGameModeVehicle : AGameModeBase` 추가
2. constructor에서 `DefaultPawnClass = AVehiclePawn::StaticClass()`
3. Project Settings의 `GameModeClassName`을 `AGameModeVehicle`로 설정
4. scene에 auto possess pawn이 없으면 GameMode가 vehicle pawn을 spawn해서 possess

초기 개발은 옵션 A가 빠르다. 기본 동작이 안정화된 뒤 옵션 B를 정식 route로 만든다.

## Step 9. Editor / Serialization

1차 구현에서는 hardcoded tuning으로 시작한다. 이후 다음 값을 `UPROPERTY(Edit)`로 노출한다.

- chassis mass
- center of mass offset
- wheel radius
- wheel width
- wheel mass
- suspension max compression
- suspension max droop
- suspension spring strength
- suspension damper rate
- engine peak torque
- clutch strength
- max steer angle
- handbrake torque
- drive type

새 파일 추가 후 필요한 작업:

- `Scripts/GenerateProjectFiles.py` 반영
- project files regeneration
- generated header/source 생성 확인
- scene save/load에서 component property가 유지되는지 확인

## Step 10. Debugging Tools

초기부터 debug draw를 넣어야 튜닝이 가능하다.

필수 debug:

- suspension ray start/end
- ray hit point
- wheel contact normal
- chassis center of mass
- wheel center offset
- current speed
- throttle/brake/steering/handbrake
- gear/rpm
- isInAir

가능하면 `VehicleDebugTypes.h`의 기존 타입을 확장해서 UI와 debug draw가 같은 데이터를 보게 한다.

## Validation Checklist

빌드:

- Debug x64 build 성공
- Release x64 build 성공
- `PhysXVehicle_static_64.lib` 링크 에러 없음

PIE:

- vehicle pawn이 possess됨
- camera가 vehicle camera로 전환됨
- W throttle
- S brake/reverse
- A/D steering
- Space handbrake
- possess되지 않은 character나 vehicle은 움직이지 않음

Physics:

- 정지 상태에서 chassis가 안정적으로 바닥 위에 있음
- suspension ray가 floor를 맞음
- 빠른 속도에서 과도한 튐이 없음
- vehicle이 자기 자신을 raycast hit하지 않음
- trigger/contact event가 기존 시스템과 충돌하지 않음

Serialization:

- vehicle pawn이 scene save/load 후 유지됨
- movement component tuning 값이 유지됨
- PIE duplicate world에서 PhysX vehicle instance가 editor world와 섞이지 않음

## Risks

- coordinate basis가 mesh forward와 다를 수 있다. PhysX basis는 고정하고 visual offset으로 해결하는 것이 안전하다.
- 기존 physics pre-sync가 vehicle chassis를 덮어쓸 수 있다. vehicle-owned actor 분리가 필요하다.
- suspension raycast filter가 부정확하면 차량이 자기 자신 또는 trigger를 밟는 문제가 생긴다.
- PhysX vehicle tuning은 SI 단위 전제가 강하다. 엔진 단위가 meter와 다르면 scale conversion이 필요하다.
- Lua input이 possession과 분리되어 있으면 여러 pawn이 동시에 입력을 받는다.

## First Milestone Scope

포함:

- PhysXVehicle 링크
- Vehicle SDK init/shutdown
- `FPhysXVehicleInstance`
- 4-wheel `PxVehicleDrive4W`
- default friction pair 1개
- suspension raycast
- input smoothing
- chassis transform sync
- wheel visual sync
- `AVehiclePawn`
- `UVehicleMovementComponent`
- PIE에서 scene-placed vehicle possess

제외:

- tank / N-wheel / no-drive vehicle
- surface별 tire friction
- advanced telemetry UI
- multiplayer replication
- editor placement UX 고도화
- wheel collision shape 정교화
- save file migration
