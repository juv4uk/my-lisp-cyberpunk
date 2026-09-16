import Codeware.*
import Codeware.UI.*

// Presentation only. It has no evaluator, host capability or policy.
public class NeuralDeckOverlay extends CustomPopup {
    public func GetQueueName() -> CName {
        return n"neuraldeck";
    }

    public func IsBlocking() -> Bool {
        return false;
    }

    public func UseCursor() -> Bool {
        return false;
    }

    protected cb func OnInitialize() {
        super.OnInitialize();
        // F10 is the only lifecycle key; cancel and Tab stay with the game.
        this.m_closeAction = n"neuraldeck_no_close_action";
    }

    protected cb func OnCreate() {
        let root = new inkCanvas();
        root.SetName(n"NeuralDeckRoot");
        root.SetAnchor(inkEAnchor.Fill);

        let panel = new inkCanvas();
        panel.SetName(n"NeuralDeckPanel");
        panel.SetAnchor(inkEAnchor.TopRight);
        panel.SetAnchorPoint(new Vector2(1.0, 0.0));
        panel.SetSize(new Vector2(620.0, 330.0));
        panel.SetMargin(new inkMargin(0.0, 64.0, 56.0, 0.0));
        panel.Reparent(root);

        let background = new inkRectangle();
        background.SetAnchor(inkEAnchor.Fill);
        background.SetOpacity(0.82);
        background.Reparent(panel);

        let title = new inkText();
        title.SetText("MY-LISP // NEURALDECK");
        title.SetFontFamily("base\\gameplay\\gui\\fonts\\orbitron\\orbitron.inkfontfamily");
        title.SetFontStyle(n"Regular");
        title.SetFontSize(26);
        title.SetLetterCase(textLetterCase.UpperCase);
        title.SetTintColor(ThemeColors.Bittersweet());
        title.SetAnchor(inkEAnchor.TopLeft);
        title.SetMargin(new inkMargin(28.0, 28.0, 0.0, 0.0));
        title.Reparent(panel);

        let divider = new inkRectangle();
        divider.SetSize(new Vector2(564.0, 2.0));
        divider.SetAnchor(inkEAnchor.TopLeft);
        divider.SetMargin(new inkMargin(28.0, 74.0, 0.0, 0.0));
        divider.SetTintColor(ThemeColors.Bittersweet());
        divider.Reparent(panel);

        let status = new inkText();
        status.SetText("CANONICAL SESSION // BRIDGE READY\nEND — CLOSE   •   GAME CONTINUES RUNNING");
        status.SetFontFamily("base\\gameplay\\gui\\fonts\\raj\\raj.inkfontfamily");
        status.SetFontStyle(n"Regular");
        status.SetFontSize(20);
        status.SetAnchor(inkEAnchor.TopLeft);
        status.SetMargin(new inkMargin(28.0, 104.0, 0.0, 0.0));
        status.Reparent(panel);

        this.SetRootWidget(root);
    }

    protected cb func OnHidden() {
        super.OnHidden();
        let service = GameInstance.GetScriptableServiceContainer()
            .GetService(n"NeuralDeckService") as NeuralDeckService;
        if IsDefined(service) {
            service.OnOverlayHidden(this);
        }
    }
}

// Codeware discovers every concrete ScriptableService during script startup.
// The service registers its hotkey when Codeware creates the service.
public class NeuralDeckService extends ScriptableService {
    private let m_overlay: ref<NeuralDeckOverlay>;
    private let m_hotkey: ref<CallbackSystemHandler>;
    private let m_closing: Bool;

    // Codeware owns engine input delivery.  Registering here avoids polling
    // Windows input from the RED4ext adapter and keeps presentation in the
    // Redscript/UI layer.  Filter in the receiver: this avoids a broken
    // function-key target filter while retaining one precise hotkey.
    private cb func OnLoad() {
        this.m_hotkey = GameInstance.GetCallbackSystem()
            .RegisterCallback(n"Input/Key", this, n"OnNeuralDeckKey", true);
        LogChannel(n"DEBUG", "my-lisp-cyberpunk: NeuralDeck registered Codeware End callback");
    }

    private cb func OnUninitialize() {
        if IsDefined(this.m_hotkey) {
            this.m_hotkey.Unregister();
            this.m_hotkey = null;
        }
        this.m_overlay = null;
    }

    private cb func OnNeuralDeckKey(event: ref<KeyInputEvent>) {
        if !Equals(event.GetAction(), EInputAction.IACT_Press) {
            return;
        }
        if !Equals(event.GetKey(), EInputKey.IK_End) {
            return;
        }
        LogChannel(n"DEBUG", "my-lisp-cyberpunk: NeuralDeck received Codeware End press");
        this.ToggleOverlay();
    }

    public func ToggleOverlay() {
        if IsDefined(this.m_overlay) {
            // Close() (Codeware CustomPopup.reds) only queues a
            // HideCustomPopupEvent; the popup does not actually detach and
            // reach OnHidden() until a fade-out animation finishes a frame
            // or more later. A second Close() call before that animation
            // completes restarts OnDetach()/the fade, so OnHideFinish (and
            // therefore our own OnHidden -> OnOverlayHidden reset) can be
            // pushed out indefinitely by repeated key presses -- observed
            // live as m_overlay staying permanently non-null after the
            // first rapid double-press, so every later press just closes a
            // phantom that never reopens. Ignore re-entrant close requests
            // instead of re-issuing Close() on an already-closing popup.
            if this.m_closing {
                LogChannel(n"DEBUG", "my-lisp-cyberpunk: NeuralDeck ToggleOverlay ignored: already closing");
                return;
            }
            LogChannel(n"DEBUG", "my-lisp-cyberpunk: NeuralDeck ToggleOverlay closing existing overlay");
            this.m_closing = true;
            this.m_overlay.Close();
            return;
        }

        let popupManager = CustomPopupManager.GetInstance();
        if !IsDefined(popupManager) {
            LogChannel(n"DEBUG", "my-lisp-cyberpunk: NeuralDeck ToggleOverlay rejected: CustomPopupManager instance missing");
            return;
        }
        if !popupManager.IsInitialized() {
            LogChannel(n"DEBUG", "my-lisp-cyberpunk: NeuralDeck ToggleOverlay rejected: CustomPopupManager not initialized");
            return;
        }

        this.m_overlay = new NeuralDeckOverlay();
        popupManager.ShowPopup(this.m_overlay);
        LogChannel(n"DEBUG", "my-lisp-cyberpunk: NeuralDeck ToggleOverlay showed popup");
    }

    public func OnOverlayHidden(overlay: ref<NeuralDeckOverlay>) {
        if Equals(this.m_overlay, overlay) {
            this.m_overlay = null;
            this.m_closing = false;
        }
    }

}
