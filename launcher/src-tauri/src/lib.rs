use serde::Serialize;
use std::fs;
use std::io::Write as _;
use std::path::{Path, PathBuf};
use std::process::Command;

#[derive(Serialize)]
struct InstallStatus {
    found: bool,
    game_dir: String,
    binary: String,
    binary_name: String,
    has_discimage: bool,
    discimage_files: Vec<String>,
    has_extracted_data: bool,
    has_userfiles: bool,
    settings_path: String,
    ini_language: Option<i32>,
    ini_prompt_style: Option<i32>,
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

fn binary_name() -> &'static str {
    if cfg!(windows) {
        "rechan.exe"
    } else {
        "rechan"
    }
}

fn launcher_dir() -> PathBuf {
    std::env::current_exe()
        .ok()
        .and_then(|p| p.parent().map(Path::to_path_buf))
        .unwrap_or_else(|| PathBuf::from("."))
}

fn config_path() -> PathBuf {
    let base = std::env::var_os("RECHAN_LAUNCHER_CONFIG")
        .map(PathBuf::from)
        .unwrap_or_else(|| launcher_dir().join("rechan-launcher.json"));
    base
}

fn read_config_dir() -> Option<String> {
    let text = fs::read_to_string(config_path()).ok()?;
    let value: serde_json::Value = serde_json::from_str(&text).ok()?;
    value.get("gameDir")?.as_str().map(str::to_owned)
}

fn write_config_dir(dir: &str) {
    let json = serde_json::json!({ "gameDir": dir }).to_string();
    let _ = fs::write(config_path(), json);
}

fn has_pc_data(dir: &Path) -> bool {
    let pc = dir.join("pc");
    pc.is_dir()
        && fs::read_dir(&pc)
            .map(|mut it| it.next().is_some())
            .unwrap_or(false)
        || dir.join("data").join("pc").is_dir()
}

fn list_disc_images(dir: &Path) -> Vec<String> {
    let mut out = Vec::new();
    for candidate in [dir.join("discimage"), dir.join("data").join("discimage")] {
        if let Ok(entries) = fs::read_dir(&candidate) {
            for entry in entries.flatten() {
                let name = entry.file_name().to_string_lossy().to_lowercase();
                let ext = Path::new(&name).extension().and_then(|e| e.to_str()).unwrap_or("");
                if matches!(ext, "bin" | "iso" | "img") {
                    out.push(entry.file_name().to_string_lossy().into_owned());
                }
            }
        }
    }
    out.sort();
    out.dedup();
    out
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
        if current.is_none() {
            continue;
        }
        let sec = lines[current.unwrap()].trim_matches(['[', ']']).trim().to_ascii_lowercase();
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
fn candidate_dirs() -> Vec<String> {
    let mut out: Vec<PathBuf> = Vec::new();
    let exe_dir = launcher_dir();
    out.push(exe_dir.clone());
    out.push(exe_dir.join("game"));
    out.push(exe_dir.join("rechan"));
    out.push(exe_dir.join("bin"));
    out.push(exe_dir.parent().unwrap_or(&exe_dir).to_path_buf());
    if let Some(docs) = dirs_documents() {
        out.push(docs.join("ReChan"));
    }
    if let Ok(env_dir) = std::env::var("RECHAN_DIR") {
        out.push(PathBuf::from(env_dir));
    }
    out.into_iter().map(|p| p.to_string_lossy().into_owned()).collect()
}

fn dirs_documents() -> Option<PathBuf> {
    std::env::var_os("HOME")
        .or_else(|| std::env::var_os("USERPROFILE"))
        .map(|h| PathBuf::from(h).join("Documents"))
}

#[tauri::command]
fn get_last_dir() -> Option<String> {
    read_config_dir()
}

#[tauri::command]
fn set_last_dir(dir: String) {
    write_config_dir(&dir);
}

#[tauri::command]
fn scan_dir(dir: String) -> InstallStatus {
    let root = PathBuf::from(&dir);
    let binary = root.join(binary_name());
    let found = binary.is_file();
    let discimage_files = list_disc_images(&root);
    let settings_path = root.join("userfiles").join("game.ini");
    let ini_text = fs::read_to_string(&settings_path).unwrap_or_default();

    InstallStatus {
        found,
        game_dir: dir,
        binary: binary.to_string_lossy().into_owned(),
        binary_name: binary_name().to_string(),
        has_discimage: !discimage_files.is_empty(),
        discimage_files,
        has_extracted_data: has_pc_data(&root),
        has_userfiles: root.join("userfiles").is_dir(),
        settings_path: settings_path.to_string_lossy().into_owned(),
        ini_language: parse_ini_value(&ini_text, "general", "language"),
        ini_prompt_style: parse_ini_value(&ini_text, "controls", "prompt_style"),
    }
}

#[tauri::command]
fn launch_game(binary: String, dir: String) -> Result<(), String> {
    let path = PathBuf::from(&binary);
    if !path.is_file() {
        return Err(format!("executável não encontrado: {binary}"));
    }
    Command::new(&path)
        .current_dir(&dir)
        .spawn()
        .map_err(|e| format!("falha ao executar: {e}"))?;
    Ok(())
}

#[tauri::command]
fn write_settings(dir: String, language: i32, prompt_style: i32) -> Result<(), String> {
    let userfiles = PathBuf::from(&dir).join("userfiles");
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
async fn fetch_releases() -> Result<Vec<ReleaseInfo>, String> {
    let client = reqwest::Client::builder()
        .user_agent("rechan-launcher")
        .build()
        .map_err(|e| e.to_string())?;
    let url = "https://api.github.com/repos/luckvice/ReChan-Ports/releases?per_page=10";
    let releases: Vec<serde_json::Value> = client
        .get(url)
        .send()
        .await
        .map_err(|e| e.to_string())?
        .json()
        .await
        .map_err(|e| e.to_string())?;

    Ok(releases
        .into_iter()
        .map(|r| ReleaseInfo {
            tag_name: r["tag_name"].as_str().unwrap_or("").to_owned(),
            name: r["name"].as_str().unwrap_or("").to_owned(),
            html_url: r["html_url"].as_str().unwrap_or("").to_owned(),
            prerelease: r["prerelease"].as_bool().unwrap_or(false),
            published_at: r["published_at"].as_str().unwrap_or("").to_owned(),
            body: r["body"].as_str().unwrap_or("").to_owned(),
        })
        .collect())
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_dialog::init())
        .plugin(tauri_plugin_opener::init())
        .invoke_handler(tauri::generate_handler![
            candidate_dirs,
            get_last_dir,
            set_last_dir,
            scan_dir,
            launch_game,
            write_settings,
            fetch_releases
        ])
        .run(tauri::generate_context!())
        .expect("error while running RE:Chan launcher");
}
