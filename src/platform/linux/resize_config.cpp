#include "resize_config.h"

#include <cctype>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace ResizeConfig {

namespace {

std::vector<HotkeyBinding> g_bindings;
std::string g_activeMode;  // "" = Fullscreen
int g_originalW = 0;
int g_originalH = 0;

// ---- Простой вычислитель выражений ----
// Поддерживает: + - * / , max() roundEven() screenWidth screenHeight числа

struct Expr {
    const char* s;
    int screenW, screenH;
    bool error;

    Expr(const char* str, int sw, int sh) : s(str), screenW(sw), screenH(sh), error(false) {}

    void skipSpaces() { while (*s == ' ') s++; }

    bool match(const char* word) {
        size_t n = strlen(word);
        if (strncmp(s, word, n) == 0) {
            char next = s[n];
            if (!isalpha(next) && !isdigit(next)) { s += n; return true; }
        }
        return false;
    }

    double parseAtom() {
        skipSpaces();
        if (match("screenWidth"))  return (double)screenW;
        if (match("screenHeight")) return (double)screenH;
        if (match("max")) {
            skipSpaces();
            if (*s == '(') s++;
            double a = parseExpr();
            skipSpaces();
            if (*s == ',') s++;
            double b = parseExpr();
            skipSpaces();
            if (*s == ')') s++;
            return (a > b) ? a : b;
        }
        if (match("roundEven")) {
            skipSpaces();
            if (*s == '(') s++;
            double v = parseExpr();
            skipSpaces();
            if (*s == ')') s++;
            int iv = (int)std::round(v);
            if (iv % 2 != 0) iv += (v > 0 ? 1 : -1);
            return (double)iv;
        }
        if (*s == '(') {
            s++;
            double v = parseExpr();
            skipSpaces();
            if (*s == ')') s++;
            return v;
        }
        // Число
        char* end;
        double v = strtod(s, &end);
        if (end > s) { s = end; return v; }
        error = true;
        return 0.0;
    }

    double parseMulDiv() {
        double v = parseAtom();
        while (!error) {
            skipSpaces();
            if (*s == '*') { s++; v *= parseAtom(); }
            else if (*s == '/') { s++; double d = parseAtom(); if (d != 0) v /= d; }
            else break;
        }
        return v;
    }

    double parseExpr() {
        double v = parseMulDiv();
        while (!error) {
            skipSpaces();
            if (*s == '+') { s++; v += parseMulDiv(); }
            else if (*s == '-') { s++; v -= parseMulDiv(); }
            else break;
        }
        return v;
    }
};

int evalExpr(const std::string& expr, int screenW, int screenH, bool& ok) {
    Expr e(expr.c_str(), screenW, screenH);
    double v = e.parseExpr();
    ok = !e.error;
    return (int)std::round(v);
}

int evalNumber(double v, int screenW, int screenH) {
    // Если v — целое число > 1, это пиксели.
    // Если v — дробь < 1, это доля от высоты экрана.
    if (v < 1.0 && v > 0.0) {
        return (int)std::round(v * (double)screenH);
    }
    return (int)std::round(v);
}

void logToFile(const char* fmt, ...) {
    FILE* f = fopen("/home/user/toolscreen.log", "a");
    if (f) {
        va_list va; va_start(va, fmt); vfprintf(f, fmt, va); va_end(va);
        fflush(f); fclose(f);
    }
}

}  // namespace

bool Load(int screenW, int screenH) {
    g_bindings.clear();

    const char* path = getenv("HOME");
    std::string fullPath;
    if (path) {
        fullPath = std::string(path) + "/.toolscreen/resize_bindings.json";
    } else {
        fullPath = "/home/user/.toolscreen/resize_bindings.json";
    }

    FILE* f = fopen(fullPath.c_str(), "r");
    if (!f) {
        logToFile("[ResizeConfig] Cannot open %s\n", fullPath.c_str());
        return false;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::string content(sz, '\0');
    fread(&content[0], 1, sz, f);
    fclose(f);

    try {
        auto json = nlohmann::json::parse(content);
        for (auto& hk : json["hotkeys"]) {
            HotkeyBinding b;
            b.keycode = hk["keycode"].get<uint32_t>();
            b.mode = hk["mode"].get<std::string>();
            bool ok = true;

            // width: может быть строкой-формулой или числом
            if (hk["width"].is_string()) {
                b.width = evalExpr(hk["width"].get<std::string>(), screenW, screenH, ok);
            } else {
                b.width = evalNumber(hk["width"].get<double>(), screenW, screenH);
            }

            // height: может быть строкой-формулой или числом
            if (ok && hk["height"].is_string()) {
                b.height = evalExpr(hk["height"].get<std::string>(), screenW, screenH, ok);
            } else if (ok) {
                b.height = evalNumber(hk["height"].get<double>(), screenW, screenH);
            }

            if (!ok) {
                logToFile("[ResizeConfig] SKIP keycode=%u: parse error in formula\n", b.keycode);
                continue;
            }

            // Кап высоты до screenHeight (16384 из EyeZoom — виртуальный вьюпорт, не окно)
            if (b.height > screenH) {
                b.height = screenH;
            }

            logToFile("[ResizeConfig] keycode=%u → %dx%d (%s)\n",
                     b.keycode, b.width, b.height,
                     hk["mode"].get<std::string>().c_str());

            g_bindings.push_back(b);
        }
    } catch (std::exception& e) {
        logToFile("[ResizeConfig] JSON parse error: %s\n", e.what());
        return false;
    }

    logToFile("[ResizeConfig] Loaded %zu bindings\n", g_bindings.size());
    return true;
}

const HotkeyBinding* Find(uint32_t keycode) {
    for (auto& b : g_bindings) {
        if (b.keycode == keycode) return &b;
    }
    return nullptr;
}

const char* GetActiveMode() {
    return g_activeMode.c_str();
}

void SetActiveMode(const char* mode) {
    g_activeMode = mode ? mode : "";
}

void SetOriginalSize(int w, int h) {
    g_originalW = w;
    g_originalH = h;
}

int GetOriginalW() { return g_originalW; }
int GetOriginalH() { return g_originalH; }

}  // namespace ResizeConfig
