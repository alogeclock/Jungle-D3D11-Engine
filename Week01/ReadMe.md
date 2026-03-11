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
Gameplay/       스테이지, 플레이어, 몬스터, 비트/스코어/아이템 시스템
Render/         D3D11 렌더러, 텍스처, 스프라이트, 비트맵 폰트
Scene/          씬 매니저, 타이틀/플레이 씬
UI/             UI 매니저, HUD 위젯, 팝업
IO/             이미지/폰트/JSON 파일 로더
Data/           스테이지/스코어 데이터 관리
ThirdParty/     ImGui
Resources/
  Fonts/        BMFont 비트맵 폰트 (.fnt + .png)
  Maps/         스테이지 맵 데이터 (JSON)
  Shaders/      HLSL 셰이더
    Default.hlsl  스프라이트 렌더링 (WVP + 아틀라스 UV)
    UI.hlsl       UI/폰트 렌더링 (스크린 좌표 → NDC)
  Sounds/       BGM / 효과음 (.wav)
  Sprites/      스프라이트 텍스처 (.png)
    Animation/  키프레임 애니메이션 데이터 (JSON)
```

## 주요 기능

- **비트 시스템**: BPM 기반 박자 판정 (Perfect / Good / Miss), 미입력 시 데미지
- **스테이지**: JSON 기반 맵 데이터, 4개 스테이지 (점진적 난이도 상승)
- **몬스터 AI**: StoneGolem / FireGolem, BFS 추적 / 랜덤 이동 / 패트롤
- **아이템**: 무적, 시간 가속/감속, 시야 밝기 조절, 시간 정지 (6종)
- **스프라이트 애니메이션**: JSON 기반 키프레임 애니메이션, 스프라이트 시트 지원
- **렌더링**: 스프라이트 배치 렌더링, 비트맵 폰트, 암흑 시야 오버레이 (5단계)
- **나침반 UI**: 출구/아이템 방향을 가리키는 나침반 위젯
- **셰이더 분리**: Default.hlsl (스프라이트) / UI.hlsl (UI/폰트) 이중 파이프라인
- **씬 관리**: 타이틀 → 플레이 씬 전환, 팝업 시스템 (게임오버/스테이지 클리어/점수 저장)
- **BGM 관리**: 씬별 BGM 재생, 씬 전환 시 명시적 StopAll/StartBGM
- **스코어보드**: 닉네임 입력 후 점수 저장, 누적 점수 시스템

## 스테이지 정보

| 스테이지 | 이름 | BPM | 맵 크기 | 제한시간 | 몬스터 | 아이템 |
|---------|------|-----|--------|---------|--------|--------|
| 1 | Rhythm Beginnings | 93 | 10×8 | 30초 | 0 | 1 |
| 2 | Double Tempo | 121 | 10×8 | 45초 | 1 | 3 |
| 3 | Jungle Labyrinth | 140 | 12×10 | 60초 | 2 | 3 |
| 4 | Jungle Echo Maze | 160 | 77×24 | 120초 | 5 | 5 |

## 빌드 방법

1. `jungle_practice.sln`을 Visual Studio에서 열기
2. 솔루션 빌드 (Ctrl+Shift+B)
3. 실행 (F5)
