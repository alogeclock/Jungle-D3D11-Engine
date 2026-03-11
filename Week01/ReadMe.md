# Jungle Week1 Team2 - Rhythm Dungeon

리듬에 맞춰 던전을 탐험하는 2D 비트 액션 게임 (DirectX 11)

## 빌드 환경

- **IDE**: Visual Studio 2019+
- **플랫폼**: Windows (x86/x64)
- **그래픽 API**: Direct3D 11
- **C++ 표준**: C++17

## 프로젝트 구조

```
Core/           앱 진입점, 윈도우, 입력, 시간, 오디오 시스템
Gameplay/       스테이지, 플레이어, 몬스터, 비트/스코어 시스템
Render/         D3D11 렌더러, 텍스처, 스프라이트, 비트맵 폰트
Scene/          씬 매니저, 타이틀/플레이 씬
UI/             UI 매니저, HUD 위젯, 팝업
IO/             이미지/폰트 파일 로더
Data/           스테이지/스코어 데이터 관리
ThirdParty/     ImGui
Resources/
  Fonts/        BMFont 비트맵 폰트 (.fnt + .png)
  Maps/         스테이지 맵 데이터 (JSON)
  Shaders/      HLSL 셰이더
    Default.hlsl  스프라이트 렌더링 (WVP + 아틀라스 UV)
    UI.hlsl       UI/폰트 렌더링 (스크린 좌표 → NDC)
  Sounds/       BGM (.wav)
  Sprites/      스프라이트 텍스처 (.png)
```

## 주요 기능

- **비트 시스템**: BPM 기반 박자 판정 (Perfect / Good / Miss)
- **스테이지**: JSON 기반 맵 데이터, 다중 스테이지 지원
- **렌더링**: 스프라이트 배치 렌더링, 비트맵 폰트, 암흑 시야 오버레이
- **셰이더 분리**: Default.hlsl (스프라이트) / UI.hlsl (UI/폰트) 이중 파이프라인
- **씬 관리**: 타이틀 → 플레이 씬 전환, 팝업 시스템

## 빌드 방법

1. `jungle_practice.sln`을 Visual Studio에서 열기
2. 솔루션 빌드 (Ctrl+Shift+B)
3. 실행 (F5)
