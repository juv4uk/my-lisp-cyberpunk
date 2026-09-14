import Codeware.*
import Codeware.UI.*

// Produced by the thin C++ input adapter and delivered through UISystem.QueueEvent.
// It carries no policy and has no Lisp semantics.
public class NeuralDeckToggleEvent extends Event {}

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
        status.SetText("CANONICAL SESSION // BRIDGE READY\nF10 — CLOSE   •   GAME CONTINUES RUNNING");
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
// The service registers its hotkey only when the game instance is initialized.
public class NeuralDeckService extends ScriptableService {
    private let m_overlay: ref<NeuralDeckOverlay>;

    private cb func OnUninitialize() {
        this.m_overlay = null;
    }

    public func ToggleOverlay() {
        if IsDefined(this.m_overlay) {
            this.m_overlay.Close();
            return;
        }

        let popupManager = CustomPopupManager.GetInstance();
        if !IsDefined(popupManager) || !popupManager.IsInitialized() {
            return;
        }

        this.m_overlay = new NeuralDeckOverlay();
        popupManager.ShowPopup(this.m_overlay);
    }

    public func OnOverlayHidden(overlay: ref<NeuralDeckOverlay>) {
        if Equals(this.m_overlay, overlay) {
            this.m_overlay = null;
        }
    }

}

// Codeware already uses this exact UISystem event pattern for its popup events.
// The event method owns presentation only; Lisp remains outside this layer.
@addMethod(PopupsManager)
protected cb func OnNeuralDeckToggle(evt: ref<NeuralDeckToggleEvent>) -> Bool {
    let service = GameInstance.GetScriptableServiceContainer()
        .GetService(n"NeuralDeckService") as NeuralDeckService;
    if !IsDefined(service) {
        return false;
    }

    service.ToggleOverlay();
    return true;
}
