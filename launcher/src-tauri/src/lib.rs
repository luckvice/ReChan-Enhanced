use serde::Serialize;
use std::fs;
use std::io::Write as _;
use std::path::{Path, PathBuf};
use tauri::Emitter;

#[derive(Serialize, Clone)]
struct GameState {
    game_dir: String,
    binary: String,
    binary_ready: bool,
    installed_version: String,
    latest_version: String,
    update_available: bool,
    ready: bool,
    has_discimage: bool,
    discimage_files: Vec<String>,
    busy: bool,
    message: String,
}

#[derive(Serialize)]
struct ReleaseInfo {
    tag_name: String,
    name: String,
    html_url: String,
    prerelease: bool,
    published_at: String,
    body: String,
}

#[derive(serde::Deserialize)]
struct GhAsset {
    name: String,
    browser_download_url: String,
}

#[derive(serde::Deserialize)]
struct GhRelease {
    tag_name: String,
    name: Option<String>,
    html_url: String,
    prerelease: bool,
    published_at: Option<String>,
    body: Option<String>,
    assets: Vec<GhAsset>,
}

fn binary_name() -> &'static str {
    if cfg!(windows) {
        "rechan.exe"
    } else {
        "rechan"
    }
}

fn documents_dir() -> PathBuf {
    std::env::var_os("HOME")
        .or_else(|| std::env::var_os("USERPROFILE"))
        .map(|h| PathBuf::from(h).join("Documents"))
        .unwrap_or_else(|| PathBuf::from("."))
}

/// The launcher owns this directory: releases are downloaded and extracted
/// here, and the disc images are linked into discimage/.
fn managed_game_dir() -> PathBuf {
    if let Ok(dir) = std::env::var("RECHAN_GAME_DIR") {
        return PathBuf::from(dir);
    }
    documents_dir().join("ReChan").join("game")
}

fn state_path(game_dir: &Path) -> PathBuf {
    game_dir.join(".rechan-game.json")
}

fn read_installed_version(game_dir: &Path) -> String {
    fs::read_to_string(state_path(game_dir))
        .ok()
        .and_then(|t| serde_json::from_str::<serde_json::Value>(&t).ok())
        .and_then(|v| v.get("version")?.as_str().map(str::to_owned))
        .unwrap_or_default()
}

fn write_installed_version(game_dir: &Path, version: &str, asset: &str) {
    let json = serde_json::json!({
        "version": version,
        "asset": asset,
        "installedAt": chrono_lite_now(),
    });
    let _ = fs::write(state_path(game_dir), json.to_string());
}

fn chrono_lite_now() -> String {
    // Avoid a datetime dependency; the field is informational only.
    String::new()
}

fn platform_token() -> String {
    let os = std::env::consts::OS;
    let arch = std::env::consts::ARCH;
    let arch_token = match arch {
        "aarch64" => "arm64",
        "x86_64" => "x64",
        other => other,
    };
    format!("{os}-{arch_token}")
}

fn is_desktop_release(tag: &str) -> bool {
    // PSP packages use tags like v1.0.0-beta2_PSP and launcher packages use
    // tags like launcher-v0.1.0; neither is a game build.
    !tag.contains("_PSP") && !tag.starts_with("launcher-")
}

async fn gh_get<T: serde::de::DeserializeOwned>(url: &str) -> Result<T, String> {
    let client = reqwest::Client::builder()
        .user_agent("rechan-launcher")
        .build()
        .map_err(|e| e.to_string())?;
    client
        .get(url)
        .send()
        .await
        .map_err(|e| e.to_string())?
        .json::<T>()
        .await
        .map_err(|e| e.to_string())
}

async fn latest_desktop_release() -> Result<GhRelease, String> {
    let releases: Vec<GhRelease> =
        gh_get("https://api.github.com/repos/luckvice/ReChan-Enhanced/releases?per_page=30").await?;
    // The latest desktop release that actually ships a binary for this
    // platform (a win-only release must not break the macOS launcher).
    releases
        .into_iter()
        .find(|r| is_desktop_release(&r.tag_name) && pick_platform_asset(r).is_some())
        .ok_or_else(|| "nenhum release desktop encontrado para esta plataforma".to_owned())
}

fn pick_platform_asset(release: &GhRelease) -> Option<&GhAsset> {
    let token = platform_token();
    release
        .assets
        .iter()
        .find(|a| a.name.to_ascii_lowercase().contains(&token.to_ascii_lowercase()))
}

fn list_disc_images(game_dir: &Path) -> Vec<String> {
    let dir = game_dir.join("discimage");
    let mut out = Vec::new();
    if let Ok(entries) = fs::read_dir(&dir) {
        for entry in entries.flatten() {
            let name = entry.file_name().to_string_lossy().to_lowercase();
            let ext = Path::new(&name).extension().and_then(|e| e.to_str()).unwrap_or("");
            if matches!(ext, "bin" | "iso" | "img") {
                out.push(entry.file_name().to_string_lossy().into_owned());
            }
        }
    }
    out.sort();
    out
}

fn build_state(game_dir: &Path, message: &str, busy: bool) -> GameState {
    let binary = game_dir.join(binary_name());
    let installed = read_installed_version(game_dir);
    GameState {
        game_dir: game_dir.to_string_lossy().into_owned(),
        binary: binary.to_string_lossy().into_owned(),
        binary_ready: binary.is_file(),
        installed_version: installed,
        latest_version: String::new(),
        update_available: false,
        ready: binary.is_file(),
        has_discimage: !list_disc_images(game_dir).is_empty(),
        discimage_files: list_disc_images(game_dir),
        busy,
        message: message.to_owned(),
    }
}

#[tauri::command]
fn game_state() -> GameState {
    let dir = managed_game_dir();
    build_state(&dir, "", false)
}

fn emit_progress(app: &tauri::AppHandle, phase: &str, received: u64, total: u64, message: &str) {
    let _ = app.emit(
        "game://progress",
        serde_json::json!({
            "phase": phase,
            "received": received,
            "total": total,
            "message": message,
        }),
    );
}

async fn download_to_file(
    app: &tauri::AppHandle,
    url: &str,
    dest: &Path,
) -> Result<u64, String> {
    let client = reqwest::Client::builder()
        .user_agent("rechan-launcher")
        .build()
        .map_err(|e| e.to_string())?;
    let mut response = client.get(url).send().await.map_err(|e| e.to_string())?;
    if !response.status().is_success() {
        return Err(format!("download falhou: HTTP {}", response.status()));
    }
    let total = response.content_length().unwrap_or(0);
    let mut file = fs::File::create(dest).map_err(|e| e.to_string())?;
    let mut received: u64 = 0;
    let mut last_emit = std::time::Instant::now();
    while let Some(chunk) = response.chunk().await.map_err(|e| e.to_string())? {
        file.write_all(&chunk).map_err(|e| e.to_string())?;
        received += chunk.len() as u64;
        if last_emit.elapsed().as_millis() > 150 {
            emit_progress(app, "downloading", received, total, "Baixando atualização…");
            last_emit = std::time::Instant::now();
        }
    }
    file.flush().map_err(|e| e.to_string())?;
    emit_progress(app, "downloading", received, total.max(received), "Download completo");
    Ok(received)
}

/// Extract the release zip into the game dir, stripping the zip's top-level
/// folder so that rechan and pc/ land directly in game_dir.
fn extract_zip(zip_path: &Path, game_dir: &Path) -> Result<(), String> {
    let file = fs::File::open(zip_path).map_err(|e| e.to_string())?;
    let mut archive = zip::ZipArchive::new(file).map_err(|e| e.to_string())?;
    for i in 0..archive.len() {
        let mut entry = archive.by_index(i).map_err(|e| e.to_string())?;
        let Some(raw) = entry.enclosed_name().map(|p| p.to_path_buf()) else {
            continue;
        };
        let mut components = raw.components();
        components.next(); // strip top-level folder
        let rel: PathBuf = components.collect();
        if rel.as_os_str().is_empty() {
            continue;
        }
        let out_path = game_dir.join(&rel);
        if entry.is_dir() || raw.to_string_lossy().ends_with('/') {
            fs::create_dir_all(&out_path).map_err(|e| e.to_string())?;
            continue;
        }
        if let Some(parent) = out_path.parent() {
            fs::create_dir_all(parent).map_err(|e| e.to_string())?;
        }
        let mut out = fs::File::create(&out_path).map_err(|e| e.to_string())?;
        std::io::copy(&mut entry, &mut out).map_err(|e| e.to_string())?;
        drop(out);
        // Restore the executable bit for the game binary.
        if rel == Path::new(binary_name()) {
            set_executable(&out_path);
        }
    }
    Ok(())
}

fn set_executable(path: &Path) {
    #[cfg(unix)]
    {
        use std::os::unix::fs::PermissionsExt;
        let _ = fs::set_permissions(path, fs::Permissions::from_mode(0o755));
    }
    #[cfg(not(unix))]
    let _ = path;
}

/// Check the latest desktop release and download/extract it when a new
/// version is available (or nothing is installed yet).
#[tauri::command]
async fn ensure_latest(app: tauri::AppHandle) -> Result<GameState, String> {
    let game_dir = managed_game_dir();
    emit_progress(&app, "checking", 0, 0, "Procurando a última versão…");
    let release = latest_desktop_release().await?;
    let installed = read_installed_version(&game_dir);

    let up_to_date = !installed.is_empty() && installed == release.tag_name;
    if up_to_date {
        let mut state = build_state(&game_dir, "Você está na última versão.", false);
        state.latest_version = release.tag_name;
        emit_progress(&app, "ready", 0, 0, "Você está na última versão.");
        return Ok(state);
    }

    let asset = pick_platform_asset(&release)
        .ok_or_else(|| format!("release {} não tem binário para {}", release.tag_name, platform_token()))?;

    fs::create_dir_all(&game_dir).map_err(|e| e.to_string())?;
    let zip_path = game_dir.join(".rechan-download.zip");
    download_to_file(&app, &asset.browser_download_url, &zip_path).await?;

    emit_progress(&app, "extracting", 0, 0, "Extraindo…");
    let tmp_dir = game_dir.join(".rechan-extract");
    let _ = fs::remove_dir_all(&tmp_dir);
    fs::create_dir_all(&tmp_dir).map_err(|e| e.to_string())?;
    extract_zip(&zip_path, &tmp_dir)?;

    // Promote extracted files over the existing install.
    for entry in fs::read_dir(&tmp_dir).map_err(|e| e.to_string())? {
        let entry = entry.map_err(|e| e.to_string())?;
        let target = game_dir.join(entry.file_name());
        if target.exists() {
            if target.is_dir() {
                let _ = fs::remove_dir_all(&target);
            } else {
                let _ = fs::remove_file(&target);
            }
        }
        fs::rename(entry.path(), &target).map_err(|e| e.to_string())?;
    }
    let _ = fs::remove_dir_all(&tmp_dir);
    let _ = fs::remove_file(&zip_path);

    write_installed_version(&game_dir, &release.tag_name, &asset.name);
    let tag = release.tag_name;
    let mut state = build_state(
        &game_dir,
        &format!("Instalado: {tag}"),
        false,
    );
    state.latest_version = tag.clone();
    emit_progress(&app, "done", 0, 0, &format!("Instalado: {tag}"));
    Ok(state)
}

/// Link (or copy) the disc images found in the chosen folder into
/// game_dir/discimage, replacing any previous images.
#[tauri::command]
fn link_disc_images(folder: String) -> Result<Vec<String>, String> {
    let src = PathBuf::from(&folder);
    if !src.is_dir() {
        return Err("pasta não encontrada".to_owned());
    }
    let mut images: Vec<PathBuf> = Vec::new();
    for entry in fs::read_dir(&src).map_err(|e| e.to_string())?.flatten() {
        let name = entry.file_name().to_string_lossy().to_lowercase();
        let ext = Path::new(&name).extension().and_then(|e| e.to_str()).unwrap_or("");
        if matches!(ext, "bin" | "iso" | "img") {
            images.push(entry.path());
        }
    }
    if images.is_empty() {
        return Err("nenhuma imagem .bin/.iso/.img nesta pasta".to_owned());
    }
    images.sort();

    let game_dir = managed_game_dir();
    let disc_dir = game_dir.join("discimage");
    fs::create_dir_all(&disc_dir).map_err(|e| e.to_string())?;
    // The game expects a single disc image in discimage/.
    for entry in fs::read_dir(&disc_dir).map_err(|e| e.to_string())?.flatten() {
        let name = entry.file_name().to_string_lossy().to_lowercase();
        let ext = Path::new(&name).extension().and_then(|e| e.to_str()).unwrap_or("");
        if matches!(ext, "bin" | "iso" | "img") {
            let _ = fs::remove_file(entry.path());
        }
    }

    let image = &images[0];
    let dest = disc_dir.join(image.file_name().unwrap());
    // Prefer a symlink (no 700 MB copy); fall back to a copy when linking
    // is not allowed (e.g. unprivileged Windows).
    #[cfg(unix)]
    let linked = std::os::unix::fs::symlink(image, &dest).is_ok();
    #[cfg(not(unix))]
    let linked = false;
    if !linked {
        fs::copy(image, &dest).map_err(|e| e.to_string())?;
    }
    Ok(list_disc_images(&game_dir))
}

#[tauri::command]
fn launch_game(dir: String) -> Result<(), String> {
    let binary = PathBuf::from(&dir).join(binary_name());
    if !binary.is_file() {
        return Err(format!("executável não encontrado: {}", binary.display()));
    }
    set_executable(&binary);
    std::process::Command::new(&binary)
        .current_dir(&dir)
        .spawn()
        .map_err(|e| format!("falha ao executar: {e}"))?;
    Ok(())
}

fn parse_ini_value(text: &str, section: &str, key: &str) -> Option<i32> {
    let mut current = String::new();
    for line in text.lines() {
        let line = line.trim();
        if line.starts_with('[') && line.ends_with(']') {
            current = line[1..line.len() - 1].trim().to_ascii_lowercase();
            continue;
        }
        if current != section.to_ascii_lowercase() {
            continue;
        }
        if let Some((k, v)) = line.split_once('=') {
            if k.trim().eq_ignore_ascii_case(key) {
                return v.trim().parse::<i32>().ok();
            }
        }
    }
    None
}

fn write_ini_value(text: &mut String, section: &str, key: &str, value: i32) {
    let mut lines: Vec<String> = text.lines().map(str::to_owned).collect();
    let mut current: Option<usize> = None;
    let mut replaced = false;

    for i in 0..lines.len() {
        let trimmed = lines[i].trim();
        if trimmed.starts_with('[') && trimmed.ends_with(']') {
            current = Some(i);
            continue;
        }
        let Some(cur) = current else { continue };
        let sec = lines[cur].trim_matches(['[', ']']).trim().to_ascii_lowercase();
        if sec != section.to_ascii_lowercase() {
            continue;
        }
        if let Some((k, _)) = trimmed.split_once('=') {
            if k.trim().eq_ignore_ascii_case(key) {
                lines[i] = format!("{} = {}", key, value);
                replaced = true;
                break;
            }
        }
    }

    if !replaced {
        let header = format!("[{}]", section);
        if let Some(idx) = lines.iter().position(|l| l.trim().eq_ignore_ascii_case(&header)) {
            lines.insert(idx + 1, format!("{} = {}", key, value));
        } else {
            if !lines.is_empty() && !lines.last().map(|l| l.trim().is_empty()).unwrap_or(true) {
                lines.push(String::new());
            }
            lines.push(header);
            lines.push(format!("{} = {}", key, value));
        }
    }

    *text = lines.join("\n") + "\n";
}

#[tauri::command]
fn read_settings() -> Result<(Option<i32>, Option<i32>), String> {
    let path = managed_game_dir().join("userfiles").join("game.ini");
    let text = fs::read_to_string(path).unwrap_or_default();
    Ok((
        parse_ini_value(&text, "general", "language"),
        parse_ini_value(&text, "controls", "prompt_style"),
    ))
}

#[tauri::command]
fn write_settings(language: i32, prompt_style: i32) -> Result<(), String> {
    let game_dir = managed_game_dir();
    let userfiles = game_dir.join("userfiles");
    fs::create_dir_all(&userfiles).map_err(|e| e.to_string())?;
    let path = userfiles.join("game.ini");
    let mut text = fs::read_to_string(&path).unwrap_or_default();
    write_ini_value(&mut text, "general", "language", language);
    write_ini_value(&mut text, "controls", "prompt_style", prompt_style);
    let mut file = fs::File::create(&path).map_err(|e| e.to_string())?;
    file.write_all(text.as_bytes()).map_err(|e| e.to_string())?;
    Ok(())
}

#[tauri::command]
fn open_game_dir() -> Result<String, String> {
    let dir = managed_game_dir();
    fs::create_dir_all(&dir).map_err(|e| e.to_string())?;
    Ok(dir.to_string_lossy().into_owned())
}

#[tauri::command]
async fn fetch_releases() -> Result<Vec<ReleaseInfo>, String> {
    let releases: Vec<GhRelease> =
        gh_get("https://api.github.com/repos/luckvice/ReChan-Enhanced/releases?per_page=10").await?;
    Ok(releases
        .into_iter()
        .map(|r| ReleaseInfo {
            tag_name: r.tag_name,
            name: r.name.unwrap_or_default(),
            html_url: r.html_url,
            prerelease: r.prerelease,
            published_at: r.published_at.unwrap_or_default(),
            body: r.body.unwrap_or_default(),
        })
        .collect())
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_dialog::init())
        .plugin(tauri_plugin_opener::init())
        .invoke_handler(tauri::generate_handler![
            game_state,
            ensure_latest,
            link_disc_images,
            launch_game,
            read_settings,
            write_settings,
            open_game_dir,
            fetch_releases
        ])
        .run(tauri::generate_context!())
        .expect("error while running RE:Chan launcher");
}
