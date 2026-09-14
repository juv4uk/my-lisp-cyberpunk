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

# Keep the boundary explicit: C++ sends a typed event, while Codeware owns the
# visible non-blocking popup lifecycle.  This catches source regressions before
# a player has to launch the game to discover that F10 has no presentation.
require_fragment("import Codeware.*" "Codeware base import")
require_fragment("import Codeware.UI.*" "Codeware UI import")
require_fragment("public class NeuralDeckToggleEvent extends Event" "typed toggle event")
require_fragment("public class NeuralDeckOverlay extends CustomPopup" "CustomPopup presentation")
require_fragment("public func IsBlocking() -> Bool {\n        return false;" "non-blocking overlay")
require_fragment("public class NeuralDeckService extends ScriptableService" "Codeware service")
require_fragment("@addMethod(PopupsManager)" "PopupsManager event receiver")
require_fragment("OnNeuralDeckToggle(evt: ref<NeuralDeckToggleEvent>) -> Bool" "typed event handler")
require_fragment("popupManager.ShowPopup(this.m_overlay)" "popup display action")
