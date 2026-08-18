# History cleanup report

## Result

- 원본 bare repository pack 합계: 약 7.35 GiB
- 정리된 14개 이력 pack 합계: 약 149.53 MiB
- 최종 monorepo pack: 약 79.33 MiB(주차 간 동일 객체 중복 제거 후)
- 감소율: 약 98.01%
- 최대 잔존 Blob: 약 18.77 MiB
- 보존한 커밋: 15,471개(저장소별 합계, 공통 조상은 중복 집계)
- 최종 고유 커밋: 15,486개(문서 커밋 1개와 병합 커밋 14개 포함)
- 보존한 브랜치: 154개
- 보존한 태그: 1개

## Per-project pack sizes

| Week | Before | After |
| --- | ---: | ---: |
| 01 | 536.82 MiB | 11.95 MiB |
| 02 | 2.51 MiB | 2.29 MiB |
| 03 | 16.98 MiB | 2.41 MiB |
| 04 | 91.38 MiB | 4.17 MiB |
| 05 | 98.66 MiB | 5.88 MiB |
| 06 | 258.11 MiB | 5.78 MiB |
| 07 | 301.89 MiB | 7.78 MiB |
| 08 | 204.37 MiB | 6.58 MiB |
| 09 | 592.15 MiB | 14.54 MiB |
| 10 | 450.62 MiB | 12.39 MiB |
| 11 | 1.14 GiB | 21.10 MiB |
| 12 | 323.69 MiB | 11.41 MiB |
| 13 | 954.19 MiB | 11.58 MiB |
| 14 | 2.47 GiB | 31.67 MiB |

## Removed from every revision

- 생성 경로: `.vs`, `Binaries`, `Intermediate`, `DerivedDataCache`, `Saved`, `Saves`, `Debug`, `Release`, `ReleaseBuild`, `x64`, `Win64`, `packages`, `.nuget`
- 빌드/덤프/아카이브: `obj`, `pdb`, `lib`, `dll`, `exe`, `dmp`, `bin`, `nupkg`, `zip` 등
- 출처를 일괄 확인할 수 없는 이미지·음원·영상: `png`, `jpg`, `wav`, `mp3`, `mp4` 등
- 가져온 모델·폰트·엔진 에셋: `fbx`, `obj`, `gltf`, `blend`, `dds`, `ttf`, `uasset`, `umap`, `skm`, `clip`, `animseq` 등
- 형식과 무관하게 25MB보다 큰 Blob

소스 폴더 안의 `Asset` 클래스와 Lua 스크립트, 설정, 셰이더, 프로젝트가 직접 생성한 직렬화 Scene 파일은 유지했습니다. 제거 규칙은 경로 전체를 무차별 삭제하지 않고 확장자와 명확한 생성 경로를 함께 사용했습니다.

## History semantics

빈 커밋과 퇴화한 병합 커밋을 제거하지 않도록 이력을 재작성했습니다. 이 때문에 파일이 모두 정리된 커밋도 작성자와 메시지를 보존합니다. 모든 프로젝트를 `WeekNN/` 아래로 이동했으므로 원본 커밋 SHA와 GPG 서명은 유지될 수 없습니다.

Windows의 대소문자 비구분 ref 충돌을 피하기 위해 모든 원본 브랜치는 `archive/weekNN/bNNN/<original-name>`으로 저장합니다. 마지막 부분이 원본 브랜치명입니다.
