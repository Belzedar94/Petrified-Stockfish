// Local profile: use the audited native engine for all playing/analysis roles.
window.addEventListener("DOMContentLoaded", () => {
  const status = document.createElement("div");
  status.id = "petrified-native-status";
  status.style.cssText =
    "position:fixed;bottom:8px;left:8px;z-index:9999;padding:8px 12px;border-radius:8px;background:#173b2b;color:white;font:14px system-ui";
  status.textContent = "Petrified-Stockfish + run4test · Conectando…";
  document.body.appendChild(status);
  const startFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
  const style = document.createElement("style");
  style.textContent = `
    .petrified-points-badge { margin-left:auto; display:flex; align-items:baseline;
      gap:7px; padding:5px 15px; border:1px solid #617247; border-radius:9px;
      background:#28391f; color:#dcf2bf; font:700 32px/1.15 system-ui;
      font-variant-numeric:tabular-nums; min-width:82px; justify-content:center; box-sizing:border-box; }
    #boardcol .playerbar { min-height:52px; gap:12px; }
    .petrified-points-badge small { font:600 12px system-ui; opacity:.85; }
    .petrified-points-badge[data-lead="true"] { background:#416521; border-color:#a6d66f; color:#fff; }
    #petrified-points-result { margin:8px 0 12px; padding:11px 14px; border-left:4px solid #91bd59;
      background:#24321e; border-radius:5px; color:#e6efd9; font:14px/1.45 system-ui; }
    #petrified-points-result strong { display:block; font-size:16px; }
    #petrified-points-result[data-pending="true"] { border-color:#d7ad58; background:#3b301c; color:#ffe5b3; }
    #petrified-points-controls label { display:inline-block; margin:4px 8px 4px 0; }
    #petrified-points-controls input { width:80px; }
  `;
  document.head.appendChild(style);
  let scoreData = null, scorePending = "Conectando el marcador…";
  const reasons = {checkmate:"Jaque mate", stalemate:"Ahogado", "fifty-move":"50 movimientos",
    "threefold-repetition":"Triple repetición", "insufficient-material":"Material insuficiente"};
  function setText(node, text) { if (node.textContent !== text) node.textContent = text; }
  function renderPoints() {
    const boardcol = document.getElementById("boardcol");
    if (!boardcol) return;
    for (const [side, selector, name] of [[0,".playerbar.bottom","Blancas"],[1,".playerbar.top","Negras"]]) {
      const player = boardcol.querySelector(selector);
      if (!player) continue;
      let badge = player.querySelector(".petrified-points-badge");
      if (!badge) {
        badge = document.createElement("div");
        badge.className = "petrified-points-badge";
        badge.id = `petrified-score-${side === 0 ? "white" : "black"}`;
        badge.innerHTML = '<span>—</span><small>pts</small>';
        player.appendChild(badge);
      }
      const value = scoreData ? String(scoreData.final[side]) : "—";
      setText(badge.firstChild, value);
      badge.setAttribute("aria-label", `${name}: ${value} puntos`);
      badge.dataset.lead = String(!!scoreData && scoreData.final[side] > scoreData.final[1-side]);
      badge.title = scoreData ? `Capturas: ${scoreData.earned[side]} · Bonus final: ${scoreData.final[side]-scoreData.earned[side]}` : scorePending;
    }
    let result = document.getElementById("petrified-points-result");
    if (!result) {
      result = document.createElement("div");
      result.id = "petrified-points-result";
      result.setAttribute("role", "status");
      result.innerHTML = "<strong></strong><span></span>";
      boardcol.insertBefore(result, boardcol.querySelector(".boardnav"));
    }
    let title = "Game of Points · Chess.com", detail = scorePending;
    if (scoreData) {
      const {earned, final, result: outcome, termination, canClaimWin} = scoreData;
      detail = "Capturas: P 1 · C 3 · A 5 · T 5 · D 9. Mate: +20; decide el total.";
      if (outcome !== "*") {
        title = `${outcome === "1-0" ? "Blancas ganan" : outcome === "0-1" ? "Negras ganan" : "Empate"} por puntos · ${final[0]}–${final[1]}`;
        detail = `${reasons[termination] || termination} · Bonus final: blancas +${final[0]-earned[0]}, negras +${final[1]-earned[1]}.`;
      } else if (canClaimWin) detail = "El jugador al turno puede reclamar la victoria por puntos.";
    }
    result.dataset.pending = String(!scoreData);
    setText(result.firstChild, title);
    setText(result.lastChild, detail);
  }
  new MutationObserver(renderPoints).observe(document.getElementById("boardcol"), {childList:true, subtree:true});
  renderPoints();
  const scores = document.createElement("fieldset");
  scores.id = "petrified-points-controls";
  scores.style.cssText = "margin:10px 0;padding:8px;border:1px solid #777";
  scores.innerHTML = '<legend>Game of Points · Chess.com</legend>' +
    '<label>Blancas · puntos anteriores al FEN <input id="petrified-white-points" type="number" min="0" max="1000000" value="0" style="width:70px"></label> ' +
    '<label>Negras <input id="petrified-black-points" type="number" min="0" max="1000000" value="0" style="width:70px"></label> ' +
    '<button id="petrified-apply-points" type="button">Aplicar marcador</button>' +
    '<div id="petrified-points-score">Las capturas de la lista de jugadas se suman automáticamente. En una posición final introduce los puntos anteriores al bonus de finalización.</div>';
  document.getElementById("input").appendChild(scores);
  let confirmedFen = "", confirmedScores = [0, 0];
  let currentPositionKey = "";
  // Keep Fairyground's game result and PGN in agreement with the native engine.
  // This small Board WASM uses the same C++ rules; it contains no evaluator.
  let pointsRules = null, installedBoardApi = false;
  const resultCache = new Map();
  const rulesScript = document.createElement("script");
  rulesScript.src = "./lib/petrified-points/ffish.js";
  rulesScript.onload = () => {
    createPetrifiedStockfish({locateFile: file => `./lib/petrified-points/${file}`}).then(api => {
      pointsRules = api;
      installBoardResults();
    }).catch(error => { status.textContent = "No se pudo cargar el árbitro del tablero: " + error; });
  };
  rulesScript.onerror = () => { status.textContent = "Falta el módulo de reglas Game of Points del tablero."; };
  document.head.appendChild(rulesScript);
  function boardResult(board) {
    if (board.variant() !== "petrified") return undefined;
    if (!pointsRules) return "*";
    const base = document.getElementById("fen").value.trim() || startFen;
    if (base !== startFen && base !== confirmedFen) return "*";
    const values = base === startFen && base !== confirmedFen ? [0, 0] : confirmedScores;
    const moves = board.moveStack();
    const key = JSON.stringify([base, values, moves]);
    let cached = resultCache.get(key);
    if (!cached) {
      let scored;
      try {
        scored = new pointsRules.Board("petrifiedpoints", `${base} points=${values.join(",")}`);
        for (const move of moves.trim().split(/\s+/).filter(Boolean)) {
          if (!scored.push(move)) return "*";
        }
        cached = {fen:scored.fen().split(" points=")[0], result:scored.result()};
        if (resultCache.size > 1024) resultCache.clear();
        resultCache.set(key, cached);
      } catch { return "*"; } finally { scored?.delete(); }
    }
    // A library/export board may belong to another FEN. Never invent its score.
    return cached.fen === board.fen() ? cached.result : "*";
  }
  function installBoardResults() {
    const api = window.ffishlib;
    if (!api || !pointsRules || installedBoardApi) return;
    installedBoardApi = true;
    const originalResult = api.Board.prototype.result;
    const originalEnd = api.Board.prototype.isGameOver;
    api.Board.prototype.result = Object.assign(function(...args) { return boardResult(this) ?? originalResult.apply(this, args); }, originalResult);
    api.Board.prototype.isGameOver = Object.assign(function(...args) {
      const result = boardResult(this);
      return result === undefined ? originalEnd.apply(this, args) : result !== "*";
    }, originalEnd);
  }
  function attachPoints(engine) {
    const scoreRequests = [];
    const setVariant = engine.SetVariant.bind(engine);
    engine.SetVariant = (variant, chess960) => setVariant(variant === "petrified" ? "petrifiedpoints" : variant, chess960);
    const setPosition = engine.SetPosition.bind(engine);
    engine.SetPosition = (fen, moves, width, height) => {
      const base = fen.trim() || startFen;
      engine.PointsReady = base === startFen || base === confirmedFen;
      if (!engine.PointsReady) {
        status.textContent = "Indica el marcador del FEN y pulsa Aplicar marcador.";
        status.style.background = "#79530c";
        if (engine.Color === "ANALYSIS" || engine === window.fairyground.BinaryEngineFeature.analysis_engine) {
          currentPositionKey = "";
          scoreData = null;
          scorePending = "Falta el marcador de esta posición. En Setup, introduce los puntos anteriores al FEN y pulsa Aplicar marcador.";
          renderPoints();
        }
        return false;
      }
      const values = base === startFen && base !== confirmedFen ? [0, 0] : confirmedScores;
      const key = JSON.stringify([base, moves, values]);
      if (engine === window.fairyground.BinaryEngineFeature.analysis_engine) currentPositionKey = key;
      scoreRequests.push(key);
      setPosition(`${base} points=${values.join(",")}`, moves, width, height);
      engine.PostMessage("points");
      return true;
    };
    const think = engine.StartThinking.bind(engine);
    engine.StartThinking = (...args) => engine.PointsReady ? think(...args) : false;
    const parse = engine.ParseEngineOutput.bind(engine);
    engine.ParseEngineOutput = (line) => {
      if (line.startsWith("info string points {")) {
        const data = JSON.parse(line.slice("info string points ".length));
        const key = scoreRequests.shift();
        if (engine === window.fairyground.BinaryEngineFeature.analysis_engine && key === currentPositionKey) {
          scoreData = data;
          renderPoints();
          document.getElementById("petrified-points-score").textContent =
            `Marcador: blancas ${data.final[0]} — negras ${data.final[1]}` +
            (data.result !== "*" ? ` · Resultado: ${data.result} (${data.termination})` :
              data.canClaimWin ? " · Se puede reclamar victoria por puntos" : "");
          status.textContent = "Petrified-Stockfish + run4test · Game of Points activo";
          status.style.background = "#173b2b";
        }
      }
      if (line === "info string points action claim-draw")
        document.getElementById("petrified-points-score").textContent += " · El motor elige cerrar con empate por puntos";
      return parse(line);
    };
  }
  document.getElementById("petrified-apply-points").addEventListener("click", () => {
    const values = ["petrified-white-points", "petrified-black-points"].map(id => Number(document.getElementById(id).value));
    if (values.some(value => !Number.isSafeInteger(value) || value < 0 || value > 1000000)) return;
    const fge = window.fairyground?.BinaryEngineFeature;
    if (!fge) return;
    const position = fge.GetBoardPosition();
    confirmedFen = position.fen.trim() || startFen;
    confirmedScores = values;
    for (const engine of [fge.first_engine, fge.second_engine, fge.analysis_engine]) {
      if (!engine?.IsLoaded) continue;
      engine.PostMessage("stop");
      engine.SetPosition(position.fen, position.moves, fge.GetBoardWidth(), fge.GetBoardHeight());
    }
    status.textContent = "Marcador aplicado. Activa el análisis para esta posición.";
    document.getElementById("setpos").click();
  });
  let attempts = 0;
  const timer = setInterval(() => {
    const fge = window.fairyground?.BinaryEngineFeature;
    installBoardResults();
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
        attachPoints(engine);
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
