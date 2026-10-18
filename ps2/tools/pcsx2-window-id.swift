// Lists on-screen (or all) windows with id, layer, owner, title and size.
// Used by pcsx2-shot.sh to find the PCSX2 game window and capture it.
//
// Usage: swift ps2/tools/pcsx2-window-id.swift [term] [--all]
//   term   optional case-insensitive filter over owner + title
//   --all  include windows that are hidden or on another Space

import CoreGraphics
import Foundation

var term = ""
var listAll = false
for arg in CommandLine.arguments.dropFirst() {
    if arg == "--all" {
        listAll = true
    } else {
        term = arg.lowercased()
    }
}

let opts: CGWindowListOption = listAll
    ? [.optionAll, .excludeDesktopElements]
    : [.optionOnScreenOnly, .excludeDesktopElements]

guard let list = CGWindowListCopyWindowInfo(opts, kCGNullWindowID) as? [[String: Any]] else {
    print("no window list")
    exit(1)
}

for w in list {
    let owner = (w[kCGWindowOwnerName as String] as? String) ?? "?"
    let name = (w[kCGWindowName as String] as? String) ?? ""
    let num = (w[kCGWindowNumber as String] as? Int) ?? 0
    let layer = (w[kCGWindowLayer as String] as? Int) ?? -1
    let onscreen = (w[kCGWindowIsOnscreen as String] as? Bool) ?? false
    let b = (w[kCGWindowBounds as String] as? [String: Any]) ?? [:]
    let bw = Int((b["Width"] as? Double) ?? 0)
    let bh = Int((b["Height"] as? Double) ?? 0)
    let hay = (owner + " " + name).lowercased()
    if term.isEmpty || hay.contains(term) {
        print("\(num)\tlayer=\(layer)\tonscreen=\(onscreen)\t\(owner)\t\"\(name)\"\t\(bw)x\(bh)")
    }
}
