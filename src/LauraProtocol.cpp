#include "LauraProtocol.h"
#include "JsonLite.h"
#include "TaskUtils.h"
#include <cwctype>

namespace LauraProtocol {
namespace {
// Stems are lowercase and without accents, as Laura compares them. A multi-word stem needs every
// word in the sentence ("kapcsol be" matches "kapcsold be" and "bekapcsolod" via "bekapcsol").
// "kezdet": phrases at the start of the sentence; the rest of the sentence is the task text.
const char kCatalog[] = R"json({"t": "katalogus", "v": 1, "parancsok": [
 {"id": "meres.be", "leiras": "Elindítja a munkaidő mérését.",
  "igek": ["indit", "inditsd", "kapcsol be", "bekapcsol", "kezd", "start", "folytat", "mehet"],
  "targyak": ["meres", "idomeres", "munkaido", "ido"]},
 {"id": "meres.ki", "leiras": "Leállítja a munkaidő mérését.",
  "igek": ["leallit", "allit le", "allitsd le", "kapcsol ki", "kikapcsol", "megallit", "allitsd meg", "stop", "allj"],
  "targyak": ["meres", "idomeres", "munkaido", "ido"]},
 {"id": "idozito.indit", "leiras": "Elindítja a munka- és pihenőidőzítőt, vagy folytatja, ha szünetel.",
  "igek": ["indit", "inditsd", "kapcsol be", "bekapcsol", "kezd", "start", "folytat", "mehet"],
  "targyak": ["idozito", "timer", "pomodoro", "munkaidozito"]},
 {"id": "idozito.szunet", "leiras": "Szünetelteti a futó időzítőt.",
  "igek": ["szuneteltet", "szunet", "megallit", "allitsd meg", "allit meg", "leallit", "allit le", "allitsd le",
   "kapcsol ki", "kikapcsol", "stop", "pause", "allj"],
  "targyak": ["idozito", "timer", "pomodoro", "munkaidozito"]},
 {"id": "idozito.kovetkezo", "leiras": "Az időzítő a következő szakaszra lép (munka után pihenő, pihenő után munka).",
  "igek": ["kovetkezo", "leptes", "leptet", "lepj", "ugorj", "ugr", "tovabb", "next", "leptes tovabb",
   "lepj tovabb", "ugorj tovabb", "menj tovabb", "vigyed tovabb"],
  "targyak": ["idozito", "timer", "pomodoro", "szakasz", "fazis"]},
 {"id": "feladat.uj", "leiras": "Új feladatot vesz fel a megadott szöveggel.", "szoveg": true,
  "kezdet": ["vegyel fel feladatot", "vegyel fel feladatnak", "vedd fel feladatnak", "vedd fel feladatkent",
   "uj feladat", "feladat", "jegyezd fel", "ird fel", "irj fel feladatot", "add hozza a feladatokhoz"]},
 {"id": "feladat.kesz", "leiras": "Késznek jelöli a megnevezett nyitott feladatot (a szöveg a feladat neve vagy egy része).",
  "szoveg": true,
  "kezdet": ["pipald ki", "jelold kesznek", "jelold meg kesznek", "jelold be kesznek", "huzd ki", "kesz a feladat",
   "kesz feladat", "feladat kesz", "feladat keszen", "elkeszult", "vegeztem a", "vegeztem ezzel", "teljesitettem"]},
 {"id": "ablak.megnyit", "leiras": "Megnyitja a TaskManager főablakát (feladatlista).",
  "igek": ["nyis meg", "nyit meg", "megnyit", "nyisd", "mutas", "mutat", "hozd elo", "hoz elo"],
  "targyak": ["taszkmenedzser", "taskmanager", "task manager", "feladatlista", "feladataim", "foablak",
   "taszkmenedzser ablak", "fo ablak"]},
 {"id": "pin.megnyit", "leiras": "Megnyitja a kitűzött (PIN) kis ablakot.",
  "igek": ["nyis meg", "nyit meg", "megnyit", "nyisd", "mutas", "mutat", "hozd elo", "hoz elo", "tuzd ki", "kituz"],
  "targyak": ["pin", "kituzott", "mini", "pin ablak", "kituzott ablak", "mini ablak"]}
]})json";

struct Entry { const char* id; Command command; };
constexpr Entry kCommands[] = {
    { "meres.be", Command::MeasureOn }, { "meres.ki", Command::MeasureOff },
    { "idozito.indit", Command::TimerStart }, { "idozito.szunet", Command::TimerPause },
    { "idozito.kovetkezo", Command::TimerNext }, { "feladat.uj", Command::TaskAdd },
    { "feladat.kesz", Command::TaskDone },
    { "ablak.megnyit", Command::OpenWindow }, { "pin.megnyit", Command::OpenPin },
};
}

const std::string& Catalog() {
    static const std::string catalog(kCatalog);
    return catalog;
}

Command Lookup(const std::string& id) {
    for (const Entry& entry : kCommands) {
        if (id == entry.id) return entry.command;
    }
    return Command::Unknown;
}

bool ParseRequest(const std::string& json, Request& request) {
    std::string type;
    if (!JsonLite::GetString(json, "t", type) || type != "parancs") return false;
    request = Request{};
    JsonLite::GetInt64(json, "req", request.req);
    JsonLite::GetString(json, "id", request.id);
    JsonLite::GetWideString(json, "szoveg", request.text);
    request.command = Lookup(request.id);
    return true;
}

std::string BuildReply(long long req, bool ok, const std::wstring& text) {
    return "{\"t\": \"valasz\", \"req\": " + std::to_string(req) + ", \"ok\": " + (ok ? "true" : "false") +
        ", \"szoveg\": \"" + TaskUtils::EscapeJsonString(TaskUtils::WideToUtf8(text)) + "\"}";
}

namespace {
// Lowercase words without accents; short words and "feladat..." (the command itself) are left out.
std::vector<std::wstring> NameWords(const std::wstring& text) {
    static const wchar_t kFrom[] = L"áéíóöőúüű";
    static const wchar_t kTo[] = L"aeiooouuu";
    std::vector<std::wstring> words;
    std::wstring word;
    auto flush = [&]() {
        if (word.size() >= 3 && word.rfind(L"feladat", 0) != 0) words.push_back(word);
        word.clear();
    };
    for (wchar_t ch : text) {
        ch = static_cast<wchar_t>(std::towlower(ch));
        if (const wchar_t* hit = wcschr(kFrom, ch); hit && ch) ch = kTo[hit - kFrom];
        if (std::iswalnum(ch)) word += ch;
        else flush();
    }
    flush();
    return words;
}

// Same word up to its ending: one is the start of the other, sharing at least 4 letters.
bool SameStem(const std::wstring& a, const std::wstring& b) {
    const std::wstring& shorter = a.size() < b.size() ? a : b;
    const std::wstring& longer = a.size() < b.size() ? b : a;
    if (shorter.size() < 4) return shorter == longer;
    return longer.compare(0, shorter.size(), shorter) == 0 ||
        (shorter.size() >= 5 && longer.compare(0, shorter.size() - 1, shorter, 0, shorter.size() - 1) == 0);
}
}

int FindOpenTask(const std::vector<Task>& tasks, const std::wstring& spoken) {
    const std::vector<std::wstring> said = NameWords(spoken);
    if (said.empty()) return 0;
    // Most of what was said must be in the name. When two names hold it equally, only one named in
    // full wins ("számla" fits "Számla" but not both "Számla befizetés" and "Számla küldés").
    int bestId = 0;
    double bestCoverage = 0.0;
    bool bestFull = false;
    bool tie = false;
    for (const Task& task : tasks) {
        if (task.completed) continue;
        const std::vector<std::wstring> name = NameWords(task.text);
        if (name.empty()) continue;
        int found = 0;
        for (const std::wstring& word : said) {
            for (const std::wstring& candidate : name) {
                if (SameStem(word, candidate)) {
                    ++found;
                    break;
                }
            }
        }
        const double coverage = static_cast<double>(found) / said.size();
        if (coverage < 0.6) continue;
        const bool full = found >= static_cast<int>(name.size());
        if (coverage > bestCoverage + 1e-9) {
            bestCoverage = coverage;
            bestId = task.id;
            bestFull = full;
            tie = false;
        } else if (coverage > bestCoverage - 1e-9) {
            if (full && !bestFull) {
                bestId = task.id;
                bestFull = true;
                tie = false;
            } else if (full == bestFull) {
                tie = true;
            }
        }
    }
    return tie ? AMBIGUOUS_TASK : bestId;
}

std::string BuildSay(const char* event, const std::wstring& text) {
    return std::string("{\"t\": \"mond\", \"esemeny\": \"") + TaskUtils::EscapeJsonString(event) +
        "\", \"szoveg\": \"" + TaskUtils::EscapeJsonString(TaskUtils::WideToUtf8(text)) + "\"}";
}

Actions Session::Adopt(HWND from) {
    Actions actions;
    if (from == peer) return actions;
    actions.unwatch = peer != nullptr;
    actions.watch = true;
    actions.changed = true;
    peer = from;
    connected = false;
    return actions;
}

Actions Session::Connect(Actions actions) {
    if (!connected) actions.changed = true;
    connected = true;
    actions.sendCatalog = true; // Again on a repeated Hello: Laura may have restarted her matcher.
    return actions;
}

Actions Session::OnHello(HWND from) {
    if (!from) return {};
    Actions actions = Adopt(from);
    if (!enabled) return actions; // Detected only: the button shows the link as switched off.
    actions.sendAck = true;
    actions.found = !connected; // Her Hello means she just started (or restarted her link).
    return Connect(actions);
}

Actions Session::OnHelloAck(HWND from) {
    if (!from) return {};
    Actions actions = Adopt(from);
    if (!enabled) {
        actions.sendBye = true; // Late answer to a Hello sent before the user switched off.
        return actions;
    }
    return Connect(actions);
}

Actions Session::OnBye(HWND from) {
    Actions actions;
    if (!from || from != peer) return actions;
    actions.unwatch = true;
    actions.changed = true;
    peer = nullptr;
    connected = false;
    return actions;
}

Actions Session::OnPeerExited() {
    Actions actions;
    if (!peer) return actions;
    actions.unwatch = true;
    actions.changed = true;
    actions.lost = connected;
    peer = nullptr;
    connected = false;
    return actions;
}

Actions Session::SetEnabled(bool on) {
    Actions actions;
    if (enabled == on) return actions;
    enabled = on;
    actions.changed = true;
    if (!peer) return actions;
    if (on) {
        actions.sendHello = true; // Laura answers HelloAck, which connects.
    } else if (connected) {
        actions.sendBye = true;
        connected = false;
    }
    return actions;
}
}
