import "./styles.css";
import { invoke } from "@tauri-apps/api/core";
import { open as openDialog } from "@tauri-apps/plugin-dialog";
import { getCurrentWindow } from "@tauri-apps/api/window";
import { openUrl, revealItemInDir } from "@tauri-apps/plugin-opener";

interface InstallStatus {
  found: boolean;
  game_dir: string;
  binary: string;
  binary_name: string;
  has_discimage: boolean;
  discimage_files: string[];
  has_extracted_data: boolean;
  has_userfiles: boolean;
  settings_path: string;
  ini_language: number | null;
  ini_prompt_style: number | null;
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

let status: InstallStatus | null = null;

function setActiveView(name: string): void {
  document.querySelectorAll<HTMLButtonElement>(".nav-btn").forEach((b) => b.classList.toggle("active", b.dataset.view === name));
  document.querySelectorAll(".view").forEach((v) => v.classList.toggle("active", v.id === `view-${name}`));
  if (name === "news") void loadNews();
}

function chip(kind: "ok" | "warn" | "bad", text: string): string {
  return `<div class="chip ${kind}"><span class="chip-dot"></span><span>${text}</span></div>`;
}

function renderStatus(s: InstallStatus | null): void {
  const grid = $("#status-grid");
  const hint = $("#play-hint");
  const play = $<HTMLButtonElement>("#btn-play");

  if (!s || !s.found) {
    grid.innerHTML = chip("bad", "Jogo não encontrado");
    hint.textContent =
      "Abra a aba Instalação e escolha a pasta que contém o executável do ReChan e os dados extraídos.";
    play.disabled = true;
    return;
  }

  const chips: string[] = [chip("ok", `Encontrado: ${s.binary_name}`)];
  chips.push(
    s.has_extracted_data
      ? chip("ok", "Assets extraídos")
      : s.has_discimage
        ? chip("warn", "Imagem de disco presente — extração na 1ª execução")
        : chip("bad", "Sem discimage/ e sem assets extraídos"),
  );
  if (s.has_discimage) {
    chips.push(chip("ok", `discimage: ${s.discimage_files.join(", ")}`));
  }
  grid.innerHTML = chips.join("");

  if (!s.has_extracted_data && !s.has_discimage) {
    hint.textContent =
      "Copie a imagem NTSC-U (SLUS-00684) para a pasta discimage/ dentro da pasta do jogo e tente de novo.";
    play.disabled = true;
  } else {
    hint.textContent = s.has_extracted_data
      ? "Pronto. A primeira execução com discimage/ extrai os assets automaticamente."
      : "Na primeira execução o jogo vai extrair os assets da imagem de disco. Mantenha o PSP... digo, o computador ligado. :)";
    play.disabled = false;
  }

  if (s.ini_language !== null) {
    $<HTMLSelectElement>("#sel-language").value = String(s.ini_language);
  }
  if (s.ini_prompt_style !== null) {
    $<HTMLSelectElement>("#sel-prompt").value = String(s.ini_prompt_style);
  }
}

async function refresh(): Promise<void> {
  const dir = await invoke<string | null>("get_last_dir");
  const candidates = await invoke<string[]>("candidate_dirs");
  let found: InstallStatus | null = null;

  const toScan = dir ? [dir, ...candidates.filter((c) => c !== dir)] : candidates;
  for (const c of toScan) {
    const s = await invoke<InstallStatus>("scan_dir", { dir: c });
    if (s.found) {
      found = s;
      break;
    }
  }

  status = found;
  $<HTMLInputElement>("#folder-path").value = found ? found.game_dir : "";
  renderStatus(found);
  renderChecklist(found);
}

function renderChecklist(s: InstallStatus | null): void {
  const list = $("#install-checklist");
  const help = $("#install-help");
  if (!s) {
    list.innerHTML = "";
    help.textContent = "Nenhuma pasta válida encontrada ainda.";
    return;
  }
  const rows: [string, "ok" | "warn" | "bad", string][] = [
    ["Executável", "ok", s.binary],
    [
      "Assets extraídos (pc/)",
      s.has_extracted_data ? "ok" : "bad",
      s.has_extracted_data ? "presentes" : "ausentes — use discimage/ ou copie os dados",
    ],
    [
      "Imagem de disco (discimage/)",
      s.has_discimage ? "ok" : "warn",
      s.has_discimage ? s.discimage_files.join(", ") : "não encontrada",
    ],
    ["Ajustes (userfiles/game.ini)", s.has_userfiles ? "ok" : "warn", s.has_userfiles ? "existe" : "será criado no 1º save"],
  ];
  list.innerHTML = rows
    .map(([label, kind, detail]) => {
      const mark = kind === "ok" ? "✔" : kind === "warn" ? "!" : "✖";
      return `<li><span class="mark ${kind}">${mark}</span><span><strong>${label}</strong> — ${detail}</span></li>`;
    })
    .join("");
  help.textContent = s.found ? "" : "Esta pasta não contém um executável do ReChan.";
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
        <h3><span class="tag">${r.prerelease ? "pre-release" : "release"}</span>${r.name || r.tag_name}</h3>
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

function wire(): void {
  $<HTMLButtonElement>("#btn-minimize").addEventListener("click", () => void getCurrentWindow().minimize());
  $<HTMLButtonElement>("#btn-close").addEventListener("click", () => void getCurrentWindow().close());

  $<HTMLElement>("#nav").addEventListener("click", (e) => {
    const btn = (e.target as HTMLElement).closest(".nav-btn") as HTMLButtonElement | null;
    if (btn?.dataset.view) setActiveView(btn.dataset.view);
  });

  $<HTMLButtonElement>("#btn-refresh").addEventListener("click", () => void refresh());

  $<HTMLButtonElement>("#btn-play").addEventListener("click", async () => {
    if (!status) return;
    try {
      await invoke("launch_game", { binary: status.binary, dir: status.game_dir });
    } catch (err) {
      $("#play-hint").textContent = `Falha ao lançar: ${String(err)}`;
    }
  });

  $<HTMLButtonElement>("#btn-open-folder").addEventListener("click", async () => {
    if (!status) return;
    try {
      await revealItemInDir(status.game_dir);
    } catch {
      /* ignore */
    }
  });

  $<HTMLButtonElement>("#btn-change-folder").addEventListener("click", async () => {
    const picked = await openDialog({ directory: true, multiple: false, title: "Pasta do jogo ReChan" });
    if (typeof picked === "string") {
      const s = await invoke<InstallStatus>("scan_dir", { dir: picked });
      if (s.found) {
        await invoke("set_last_dir", { dir: picked });
      }
      status = s.found ? s : null;
      $<HTMLInputElement>("#folder-path").value = s.game_dir;
      renderStatus(status);
      renderChecklist(status);
      if (!s.found) {
        $("#install-help").textContent = "Não encontrei o executável (rechan / rechan.exe) nesta pasta.";
      }
    }
  });

  $<HTMLButtonElement>("#btn-save-settings").addEventListener("click", async () => {
    const msg = $("#settings-msg");
    if (!status?.found) {
      msg.textContent = "Escolha uma pasta do jogo válida primeiro (aba Instalação).";
      return;
    }
    const language = Number($<HTMLSelectElement>("#sel-language").value);
    const promptStyle = Number($<HTMLSelectElement>("#sel-prompt").value);
    try {
      await invoke("write_settings", { dir: status.game_dir, language, promptStyle });
      msg.textContent = "Salvo em userfiles/game.ini.";
    } catch (err) {
      msg.textContent = `Erro ao salvar: ${String(err)}`;
    }
  });

  const openRepo = (url: string) => () => void openUrl(url);
  $<HTMLButtonElement>("#btn-open-repo").addEventListener("click", openRepo("https://github.com/luckvice/ReChan-Ports"));
  $<HTMLButtonElement>("#btn-open-issues").addEventListener("click", openRepo("https://github.com/luckvice/ReChan-Ports/issues"));
  $<HTMLButtonElement>("#btn-open-releases").addEventListener("click", openRepo("https://github.com/luckvice/ReChan-Ports/releases"));
}

wire();
void refresh();
