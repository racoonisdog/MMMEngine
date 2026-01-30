# UserScripts 작성법

유저 스크립트(UserScripts.dll)를 작성할 때 지켜야 할 규칙과 매크로 사용법을 정리한 문서입니다.  
에디터에서 **스크립트 빌드**를 실행하면 헤더 분석기와 코드 생성기가 자동으로 RTTR 등록 및 메시지 등록 코드를 생성합니다.

---

## 1. 파일 구조

- **위치**: **게임 프로젝트** 루트(에디터에서 연 프로젝트 폴더) 기준  
  `Source/UserScripts/Scripts/` 아래 (하위 폴더 가능)
- **파일**: 클래스당 `.h` 한 개 + `.cpp` 한 개
- **자동 생성 파일**
  - **경로**: `Source/UserScripts/Scripts/UserScripts.gen.cpp`  
    (엔진이 아니라 **현재 연 게임 프로젝트** 폴더 안)
  - 스크립트 빌드 시 **항상** 생성기가 생성·덮어씀 (스크립트가 0개여도 빈 gen.cpp 생성)
  - **수동 편집 금지**

---

## 2. 헤더(.h) 작성

### 2.1 기본 형태

```cpp
#include "rttr/type"
#include "ScriptBehaviour.h"
#include "UserScriptsCommon.h"

namespace MMMEngine
{
    class USERSCRIPTS MyScript : public ScriptBehaviour
    {
    private:
        RTTR_ENABLE(ScriptBehaviour)
        RTTR_REGISTRATION_FRIEND
    public:
        MyScript()
        {
            // 비워 둠. 빌드 시 생성기가 REGISTER_BEHAVIOUR_MESSAGE(...) 를 자동 주입함
        }

        void Start();
        void Update();
    };
}
```

- **생성자 본문**  
  - `REGISTER_BEHAVIOUR_MESSAGE(...)` 는 **생성기가 자동 주입**하므로 직접 적지 않습니다.  
  - **ExecutionOrder** 나 그 밖의 코드는 **생성자가 그대로 유지**합니다.  
    `SetExecutionOrder(100);` 또는 `USCRIPT_EXECUTION_ORDER(100);` 를 생성자 안에 적어 두면, 빌드 시 삭제되지 않고 유지됩니다.

### 2.2 메시지 등록 규칙 (BehaviourManager가 호출하는 함수)

다음 두 가지 중 하나만 만족하면 **자동으로** 메시지 등록 대상이 됩니다.

| 조건 | 설명 |
|------|------|
| **1. 엔진 시그니처와 일치** | 함수 이름·파라미터가 엔진 하드코딩 시그니처와 같으면 **매크로 없이** 자동 등록 |
| **2. 매크로 사용** | `USCRIPT_MESSAGE()` 또는 `USCRIPT_MESSAGE_NAME("이름")` 을 붙이면 **항상** 등록 (커스텀 메시지 포함) |

- **매크로 없이** 엔진 시그니처와 일치하는 함수만 쓰면 → 그대로 두면 됨 (생성기가 처리).
- **커스텀 메시지**(엔진 목록에 없는 이름/시그니처)는 반드시 **매크로**를 붙여야 등록됨.

#### 매크로 종류

| 매크로 | 용도 |
|--------|------|
| `USCRIPT_MESSAGE()` | 메시지 이름 = 함수 이름으로 등록. 함수 선언 **바로 위** 한 줄에 적음 |
| `USCRIPT_MESSAGE_NAME("이름")` | 메시지 이름을 `"이름"` 으로 등록 (함수 이름과 다르게 쓸 때) |

#### 예시

```cpp
// 엔진 시그니처와 일치 → 매크로 없이 자동 등록
void Start();
void Update();
void FixedUpdate();
void OnCollisionEnter(const CollisionInfo& info);

// 커스텀 메시지 → 매크로 필수
USCRIPT_MESSAGE()
void OnJump();

USCRIPT_MESSAGE_NAME("OnCustomEvent")
void HandleCustomEvent(int value);
```

### 2.3 엔진에서 자동 인식하는 메시지 시그니처 (매크로 없이 등록 가능)

아래와 **이름·파라미터가 일치**하면 매크로 없이 등록됩니다.  
(파라미터는 `const T&`, `T` 등 정규화 후 비교)

**void()**

- `Awake`, `OnEnable`, `Start`, `OnDisable`, `OnDestroy`
- `FixedUpdate`, `Update`, `LateUpdate`
- `OnSceneLoaded`

**void(CollisionInfo) / void(const CollisionInfo&)**

- `OnCollisionEnter`, `OnCollisionStay`, `OnCollisionExit`

**void(TriggerInfo) / void(const TriggerInfo&)**

- `OnTriggerEnter`, `OnTriggerExit`

---

### 2.4 인스펙터/리플렉션 프로퍼티

| 매크로 | 용도 |
|--------|------|
| `USCRIPT_PROPERTY()` | 인스펙터에 노출 + gen.cpp 에서 RTTR `.property(...)` 자동 등록 |
| `USCRIPT_PROPERTY_HIDDEN()` | 프로퍼티이지만 **자동 등록 제외** (gen.cpp 에 넣지 않음) |

```cpp
USCRIPT_PROPERTY()
float moveSpeed = 5.0f;

USCRIPT_PROPERTY()
std::string characterName;

USCRIPT_PROPERTY_HIDDEN()
bool debugFlag = false;
```

---

### 2.5 ExecutionOrder (생성자에서만 설정 가능)

`SetExecutionOrder(int)` 는 Behaviour 기준으로 호출 순서를 정할 때 쓰며, **생성자에서만** 호출할 수 있습니다.

- **직접 호출**: 생성자 안에 `SetExecutionOrder(100);` 처럼 적으면 됩니다.  
  생성기는 `REGISTER_BEHAVIOUR_MESSAGE` 가 아닌 라인을 **그대로 유지**하므로, ExecutionOrder 설정은 삭제되지 않습니다.
- **매크로(선택)**: `USCRIPT_EXECUTION_ORDER(100);` 는 `SetExecutionOrder(100);` 로 치환되는 편의 매크로입니다.  
  생성기 쪽에서 따로 파싱할 필요 없이, 그 한 줄이 그대로 보존됩니다.

```cpp
MyScript()
{
    USCRIPT_EXECUTION_ORDER(100);  // 또는 SetExecutionOrder(100);
}
```

### 2.6 자동 생성 스킵 (직접 작성할 때)

리플렉션/생성자/RTTR을 **전부 직접** 작성하는 클래스는 생성기를 건너뛰려면 클래스 안에 다음 매크로를 넣습니다.

```cpp
USCRIPT_DONT_AUTOGEN()
```

- 이 클래스는 **gen.cpp 에 포함되지 않고**, 생성자 주입도 하지 않습니다.

---

## 3. 소스(.cpp) 작성

### 3.1 gen.cpp 에 맡기는 경우 (권장)

RTTR 등록을 **자동 생성(gen.cpp)** 에 맡기면 `.cpp` 에는 **구현만** 넣습니다.

```cpp
#include "Export.h"
#include "ScriptBehaviour.h"
#include "MyScript.h"

void MMMEngine::MyScript::Start()
{
}

void MMMEngine::MyScript::Update()
{
}
```

- **RTTR_PLUGIN_REGISTRATION** 블록을 적지 않습니다.  
  `UserScripts.gen.cpp` 가 이 클래스를 포함해 RTTR 등록합니다.

### 3.2 RTTR을 직접 작성하는 경우

해당 클래스의 `.cpp` 에 **RTTR_PLUGIN_REGISTRATION** 과 `registration::class_<MyScript>` 등을 직접 적으면,  
생성기는 **그 클래스만 gen.cpp 에 넣지 않고** 스킵합니다. (중복 등록 방지)

- **생성자 안의 REGISTER_BEHAVIOUR_MESSAGE** 는 여전히 생성기가 주입합니다.  
  RTTR만 직접 작성하고, 메시지 등록은 그대로 빌드 시 자동 주입됩니다.

---

## 4. 빌드 시 자동으로 일어나는 일

1. **헤더 분석**  
   `Scripts/**/*.h` 에서 `ScriptBehaviour` 상속 클래스, `USCRIPT_*` 매크로, 엔진 시그니처 일치 함수를 수집합니다.

2. **UserScripts.gen.cpp 생성**  
   - 수집한 클래스 중 **자기 .cpp 에 RTTR이 없는 클래스만** gen.cpp 에 포함.
   - `#include`, `RTTR_PLUGIN_REGISTRATION`, `registration::class_<T>`, `.property(...)`, `ObjPtr<T>` 등록을 자동 생성.

3. **생성자 주입**  
   - 각 스크립트 헤더의 `ClassName() { }` 를 찾아,  
     수집한 메시지 목록으로 `REGISTER_BEHAVIOUR_MESSAGE(함수이름);` 를 정렬해서 넣습니다.  
   - 이미 있는 `REGISTER_BEHAVIOUR_MESSAGE` 는 재구성하고, 그 외 라인(예: `SetExecutionOrder`)은 유지합니다.

---

## 5. 체크리스트 (팀 공유용)

- [ ] 스크립트 클래스는 `ScriptBehaviour` 상속, `USERSCRIPTS` / `RTTR_ENABLE` / `RTTR_REGISTRATION_FRIEND` 유지
- [ ] 생성자 본문은 **비워 두기** (REGISTER_BEHAVIOUR_MESSAGE 는 생성기가 주입)
- [ ] 엔진 제공 메시지(Start, Update 등)는 매크로 없이 선언 가능
- [ ] **커스텀 메시지**는 반드시 `USCRIPT_MESSAGE()` 또는 `USCRIPT_MESSAGE_NAME("이름")` 사용
- [ ] 인스펙터에 넣을 멤버는 `USCRIPT_PROPERTY()`, 제외할 멤버는 `USCRIPT_PROPERTY_HIDDEN()`
- [ ] RTTR을 gen.cpp 에 맡기면 `.cpp` 에는 구현만 두고, RTTR 블록은 적지 않기
- [ ] `UserScripts.gen.cpp` 는 수동 편집하지 않기

---

## 6. 참고

- 엔진 시그니처 목록: `MMMEngineShared/UserScriptMessageSignatures.cpp`
- 매크로 정의: `MMMEngineShared/UserScriptsCommon.h`
- 생성기: `MMMEngineEditor/UserScriptsGenerator.cpp` (빌드 시 `BuildManager::BuildUserScripts` 직전 실행)
