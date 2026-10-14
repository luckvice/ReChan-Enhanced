import "./styles.css";
import { invoke } from "@tauri-apps/api/core";
import { listen } from "@tauri-apps/api/event";
import { open as openDialog } from "@tauri-apps/plugin-dialog";
import { getCurrentWindow } from "@tauri-apps/api/window";
import { openUrl, revealItemInDir } from "@tauri-apps/plugin-opener";

interface GameState {
  game_dir: string;
  binary: string;
  binary_ready: boolean;
  installed_version: string;
  latest_version: string;
  update_available: boolean;
  ready: boolean;
  has_discimage: boolean;
  discimage_files: string[];
  busy: boolean;
  message: string;
}

interface ProgressPayload {
  phase: string;
  received: number;
  total: number;
  message: string;
}

interface ReleaseInfo {
  tag_name: string;
  name: string;
  html_url: string;
  prerelease: boolean;
  published_at: string;
  body: string;
}

const $ = <T extends HTMLElement>(sel: string): T => document.querySelector(sel) as T;

let state: GameState | null = null;
let busy = false;

function setActiveView(name: string): void {
  document.querySelectorAll<HTMLButtonElement>(".nav-btn").forEach((b) => b.classList.toggle("active", b.dataset.view === name));
  document.querySelectorAll(".view").forEach((v) => v.classList.toggle("active", v.id === `view-${name}`));
  if (name === "news") void loadNews();
}

function chip(kind: "ok" | "warn" | "bad" | "busy", text: string): string {
  return `<div class="chip ${kind}"><span class="chip-dot"></span><span>${text}</span></div>`;
}

function renderState(s: GameState | null): void {
  const grid = $("#status-grid");
  const hint = $("#play-hint");
  const play = $<HTMLButtonElement>("#btn-play");

  if (!s) {
    grid.innerHTML = chip("bad", "Estado desconhecido");
    play.disabled = true;
    return;
  }

  const chips: string[] = [];
  chips.push(
    s.installed_version
      ? chip("ok", `Instalado: ${s.installed_version}`)
      : chip("warn", "Jogo não instalado"),
  );
  chips.push(s.binary_ready ? chip("ok", "Binário pronto") : chip("warn", "Baixando binário…"));
  chips.push(
    s.has_discimage
      ? chip("ok", `Disco: ${s.discimage_files.join(", ")}`)
      : chip("warn", "Selecione a pasta com o disco"),
  );
  grid.innerHTML = chips.join("");

  if (!s.binary_ready) {
    hint.textContent = "O launcher está baixando a última versão automaticamente…";
    play.disabled = true;
  } else if (!s.has_discimage) {
    hint.textContent =
      "Falta a imagem do disco. Abra a aba Disco e selecione a pasta com o arquivo .bin/.iso do seu disco legal (SLUS-00684).";
    play.disabled = false;
  } else {
    hint.textContent =
      "Pronto. Na primeira execução o jogo extrai os assets da imagem de disco; depois disso é só jogar.";
    play.disabled = false;
  }

  const msg = $("#play-msg");
  msg.textContent = s.message && !s.busy ? s.message : "";
}

function showProgress(p: ProgressPayload): void {
  const box = $("#progress");
  const bar = $("#progress-bar");
  const text = $("#progress-text");
  if (p.phase === "ready" || p.phase === "done") {
    box.hidden = true;
    return;
  }
  box.hidden = false;
  const labels: Record<string, string> = {
    checking: "Verificando atualizações…",
    downloading: "Baixando…",
    extracting: "Extraindo…",
  };
  text.textContent = p.message || labels[p.phase] || p.phase;
  if (p.total > 0) {
    bar.style.width = `${Math.min(100, Math.round((p.received / p.total) * 100))}%`;
  } else {
    bar.style.width = "100%";
    bar.classList.add("indeterminate");
  }
}

async function runEnsureLatest(auto: boolean): Promise<void> {
  if (busy) return;
  busy = true;
  $<HTMLButtonElement>("#btn-refresh").disabled = true;
  $("#status-grid").innerHTML = chip("busy", "Verificando atualizações…");
  try {
    const s = await invoke<GameState>("ensure_latest");
    state = s;
  } catch (err) {
    const fallback = await invoke<GameState>("game_state").catch(() => null);
    state = fallback;
    $("#play-msg").textContent = `Falha ao atualizar: ${String(err)}`;
    if (auto && fallback) {
      // Offline or rate-limited: still allow playing the installed copy.
      $("#play-msg").textContent += " (modo offline: usando a versão instalada)";
    }
  } finally {
    busy = false;
    $<HTMLButtonElement>("#btn-refresh").disabled = false;
    $("#progress").hidden = true;
    $("#progress-bar").classList.remove("indeterminate");
    renderState(state);
    renderDisc();
  }
}

function renderDisc(): void {
  const list = $("#disc-list");
  if (!state || state.discimage_files.length === 0) {
    list.innerHTML = `<li><span class="mark warn">!</span><span>Nenhuma imagem vinculada em <code>discimage/</code></span></li>`;
    return;
  }
  list.innerHTML = state.discimage_files
    .map(
      (f) =>
        `<li><span class="mark ok">✔</span><span><code>discimage/${f}</code></span></li>`,
    )
    .join("");
}

async function loadSettings(): Promise<void> {
  try {
    const [language, promptStyle] = await invoke<(number | null)[]>("read_settings");
    if (language !== null) $<HTMLSelectElement>("#sel-language").value = String(language);
    if (promptStyle !== null) $<HTMLSelectElement>("#sel-prompt").value = String(promptStyle);
  } catch {
    /* first run: defaults stay */
  }
}

async function loadNews(): Promise<void> {
  const el = $("#news-list");
  try {
    const releases = await invoke<ReleaseInfo[]>("fetch_releases");
    if (releases.length === 0) {
      el.textContent = "Nenhum release publicado.";
      return;
    }
    el.innerHTML = releases
      .map(
        (r) => `
      <div class="news-item">
        <h3><span class="tag">${r.prerelease ? "pre-release" : "release"}</span>${escapeHtml(r.name || r.tag_name)}</h3>
        <div class="meta">${r.tag_name} · ${r.published_at ? r.published_at.slice(0, 10) : ""} · <a href="${r.html_url}" data-url="${r.html_url}">ver no GitHub</a></div>
        <div class="news-body">${escapeHtml((r.body || "").slice(0, 900))}</div>
      </div>`,
      )
      .join("");
    el.querySelectorAll<HTMLAnchorElement>("a[data-url]").forEach((a) => {
      a.addEventListener("click", (e) => {
        e.preventDefault();
        void openUrl(a.dataset.url!);
      });
    });
  } catch (err) {
    el.textContent = `Falha ao carregar: ${String(err)}`;
  }
}

function escapeHtml(s: string): string {
  return s.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
}

async function openGameFolder(): Promise<void> {
  try {
    const dir = await invoke<string>("open_game_dir");
    await revealItemInDir(dir);
  } catch (err) {
    $("#play-msg").textContent = String(err);
  }
}

function wire(): void {
  $<HTMLButtonElement>("#btn-minimize").addEventListener("click", () => void getCurrentWindow().minimize());
  $<HTMLButtonElement>("#btn-close").addEventListener("click", () => void getCurrentWindow().close());

  $<HTMLElement>("#nav").addEventListener("click", (e) => {
    const btn = (e.target as HTMLElement).closest(".nav-btn") as HTMLButtonElement | null;
    if (btn?.dataset.view) setActiveView(btn.dataset.view);
  });

  $<HTMLButtonElement>("#btn-refresh").addEventListener("click", () => void runEnsureLatest(false));

  $<HTMLButtonElement>("#btn-play").addEventListener("click", async () => {
    if (!state) return;
    try {
      await invoke("launch_game", { dir: state.game_dir });
    } catch (err) {
      $("#play-msg").textContent = `Falha ao lançar: ${String(err)}`;
    }
  });

  $<HTMLButtonElement>("#btn-open-folder").addEventListener("click", () => void openGameFolder());
  $<HTMLButtonElement>("#btn-open-game-folder").addEventListener("click", () => void openGameFolder());

  $<HTMLButtonElement>("#btn-pick-disc").addEventListener("click", async () => {
    const folder = await openDialog({ directory: true, multiple: false, title: "Pasta com a imagem do disco" });
    if (typeof folder !== "string") return;
    try {
      const files = await invoke<string[]>("link_disc_images", { folder });
      $("#disc-msg").textContent = `Vinculado: ${files.join(", ")}`;
      state = await invoke<GameState>("game_state");
      renderState(state);
      renderDisc();
    } catch (err) {
      $("#disc-msg").textContent = String(err);
    }
  });

  $<HTMLButtonElement>("#btn-save-settings").addEventListener("click", async () => {
    const language = Number($<HTMLSelectElement>("#sel-language").value);
    const promptStyle = Number($<HTMLSelectElement>("#sel-prompt").value);
    try {
      await invoke("write_settings", { language, promptStyle });
      $("#settings-msg").textContent = "Salvo em userfiles/game.ini.";
    } catch (err) {
      $("#settings-msg").textContent = `Erro ao salvar: ${String(err)}`;
    }
  });

  const openRepo = (url: string) => () => void openUrl(url);
  $<HTMLButtonElement>("#btn-open-repo").addEventListener("click", openRepo("https://github.com/luckvice/ReChan-Enhanced"));
  $<HTMLButtonElement>("#btn-open-issues").addEventListener("click", openRepo("https://github.com/luckvice/ReChan-Enhanced/issues"));
  $<HTMLButtonElement>("#btn-open-releases").addEventListener("click", openRepo("https://github.com/luckvice/ReChan-Enhanced/releases"));
}

async function boot(): Promise<void> {
  wire();
  await listen<ProgressPayload>("game://progress", (e) => showProgress(e.payload));
  void loadSettings();
  state = await invoke<GameState>("game_state").catch(() => null);
  renderState(state);
  renderDisc();
  // Automatic: detect the latest release and download/update on first run.
  await runEnsureLatest(true);
}

void boot();
