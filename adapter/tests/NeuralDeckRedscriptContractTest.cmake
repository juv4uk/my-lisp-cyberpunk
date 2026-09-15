if(NOT DEFINED INPUT OR NOT EXISTS "${INPUT}")
  message(FATAL_ERROR "INPUT must name NeuralDeckOverlay.reds")
endif()

file(READ "${INPUT}" SOURCE)

function(require_fragment FRAGMENT DESCRIPTION)
  string(FIND "${SOURCE}" "${FRAGMENT}" FOUND_AT)
  if(FOUND_AT EQUAL -1)
    message(FATAL_ERROR "NeuralDeck Redscript contract missing: ${DESCRIPTION}")
  endif()
endfunction()

# Codeware owns both the input callback and the visible non-blocking popup
# lifecycle. C++ must not attempt to invoke compiled Redscript UI methods.
# This catches source regressions before a player has to launch the game to
# discover that F10 has no presentation.
require_fragment("import Codeware.*" "Codeware base import")
require_fragment("import Codeware.UI.*" "Codeware UI import")
require_fragment("public class NeuralDeckOverlay extends CustomPopup" "CustomPopup presentation")
require_fragment("public func IsBlocking() -> Bool {\n        return false;" "non-blocking overlay")
require_fragment("public class NeuralDeckService extends ScriptableService" "Codeware service")
require_fragment("private cb func OnLoad()" "service load lifecycle")
require_fragment("GameInstance.GetCallbackSystem()" "Codeware callback system")
require_fragment("RegisterCallback(n\"Input/Key\", this, n\"OnNeuralDeckKey\", true)" "F10 callback registration")
require_fragment("InputTarget.Key(EInputKey.IK_F10, EInputAction.IACT_Press)" "F10 press filter")
require_fragment("private cb func OnNeuralDeckKey(event: ref<KeyInputEvent>)" "F10 callback receiver")
require_fragment("this.m_hotkey.Unregister();" "F10 callback cleanup")
require_fragment("popupManager.ShowPopup(this.m_overlay)" "popup display action")

if(NOT DEFINED ADAPTER_INPUT OR NOT EXISTS "${ADAPTER_INPUT}")
  message(FATAL_ERROR "ADAPTER_INPUT must name adapter/src/Main.cpp")
endif()
file(READ "${ADAPTER_INPUT}" ADAPTER_SOURCE)
foreach(FORBIDDEN "GetAsyncKeyState" "ToggleFromNative" "NeuralDeckBridge")
  string(FIND "${ADAPTER_SOURCE}" "${FORBIDDEN}" FOUND_AT)
  if(NOT FOUND_AT EQUAL -1)
    message(FATAL_ERROR "NeuralDeck ownership regression: C++ adapter contains ${FORBIDDEN}")
  endif()
endforeach()
