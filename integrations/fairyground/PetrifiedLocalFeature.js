// Local profile: use the audited native engine for all playing/analysis roles.
window.addEventListener("DOMContentLoaded", () => {
  const status = document.createElement("div");
  status.id = "petrified-native-status";
  status.style.cssText =
    "position:fixed;bottom:8px;left:8px;z-index:9999;padding:8px 12px;border-radius:8px;background:#173b2b;color:white;font:14px system-ui";
  status.textContent = "Petrified-Stockfish + run4test · Conectando…";
  document.body.appendChild(status);
  let attempts = 0;
  const timer = setInterval(() => {
    const fge = window.fairyground?.BinaryEngineFeature;
    if (++attempts > 120) {
      clearInterval(timer);
      status.textContent = "Motor local no conectado. Reinicia Fairyground.";
      status.style.background = "#842c2c";
      return;
    }
    if (!fge || fge.WebSocketStatus !== "CONNECTED") return;
    clearInterval(timer);
    fge.GetEngineList((list) => {
      const config = list.find(
        (item) => item.id === "Petrified-Stockfish + run4test",
      );
      if (!config) {
        status.textContent = "Falta el perfil de Petrified-Stockfish.";
        return;
      }
      let loaded = 0;
      for (const [role, key] of [
        ["WHITE", "first_engine"],
        ["BLACK", "second_engine"],
        ["ANALYSIS", "analysis_engine"],
      ]) {
        const engine = new fge.Engine(
          config.id,
          config.path,
          config.working_directory,
          config.protocol,
          config.options,
          role,
          15000,
          fge.ws,
        );
        fge[key] = engine;
        engine.Load(
          () => {
            if (!engine.SetVariant("petrified", false)) {
              status.textContent =
                "El motor no anuncia soporte UCI de Petrified.";
              status.style.background = "#842c2c";
              return;
            }
            const position = fge.GetBoardPosition();
            engine.SetPosition(
              position.fen,
              position.moves,
              fge.GetBoardWidth(),
              fge.GetBoardHeight(),
            );
            if (++loaded === 3) {
              document.getElementById("binengineoutputinit").click();
              for (const id of [
                "whiteunsupportedvariant",
                "blackunsupportedvariant",
                "analysisunsupportedvariant",
              ])
                document.getElementById(id).hidden = true;
              status.textContent =
                "Petrified-Stockfish + run4test · Motor nativo listo";
            }
          },
          (error) => {
            status.textContent = "Error al cargar el motor: " + error;
            status.style.background = "#842c2c";
          },
        );
      }
    }, fge.ws);
  }, 500);
});
