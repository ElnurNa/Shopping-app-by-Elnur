// Shopping app logic in C++, compiled to WebAssembly with Emscripten.
// The browser (index.html) calls the extern "C" functions at the bottom.
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE   // lets the file also compile with plain g++
#endif
using namespace std;

struct Product {
    string name;
    double price;
    int quantity;
};

struct Item {          // a Product plus an id so the page can remove it
    int id;
    Product product;
};

static vector<Item> items;
static int nextId = 1;
static string result;  // text returned to JavaScript
static const int MAX_PRODUCTS = 100;

// ---------- helpers ----------
static string toLower(string s) {
    for (char &c : s) c = tolower(static_cast<unsigned char>(c));
    return s;
}

static string trim(const string &s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

static string jsonEscape(const string &s) {
    string out;
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c >= 0x20) out += static_cast<char>(c);
        }
    }
    return out;
}

static string num(double v) {
    char buf[64];
    snprintf(buf, sizeof buf, "%.2f", v);
    return buf;
}

static string itemJson(const Item &it) {
    const Product &p = it.product;
    return "{\"id\":" + to_string(it.id) +
           ",\"name\":\"" + jsonEscape(p.name) + "\"" +
           ",\"price\":" + num(p.price) +
           ",\"quantity\":" + to_string(p.quantity) +
           ",\"subtotal\":" + num(p.price * p.quantity) + "}";
}

// 10% replaces 5% (they do not stack)
static int discountFor(double total) {
    if (total > 1000) return 10;
    if (total > 500) return 5;
    return 0;
}

// ---------- functions called from JavaScript ----------
extern "C" {

// Returns 0 on success, or an error code:
// 1 = bad name, 2 = bad price, 3 = bad quantity, 4 = list is full
EMSCRIPTEN_KEEPALIVE
int add_product(const char *rawName, double price, int quantity) {
    string name = trim(rawName ? rawName : "");
    if (name.empty() || name.size() > 100) return 1;
    if (!isfinite(price) || price < 0) return 2;
    if (quantity < 1) return 3;
    if ((int)items.size() >= MAX_PRODUCTS) return 4;

    items.push_back(Item{nextId++, Product{name, price, quantity}});
    return 0;
}

// Returns 1 if the product was removed, 0 if the id was not found
EMSCRIPTEN_KEEPALIVE
int remove_product(int id) {
    auto it = find_if(items.begin(), items.end(),
                      [id](const Item &x) { return x.id == id; });
    if (it == items.end()) return 0;
    items.erase(it);
    return 1;
}

// JSON: items, subtotal, discount, total, most expensive product
EMSCRIPTEN_KEEPALIVE
const char *get_receipt() {
    double subtotal = 0;
    string list;
    int best = -1;
    for (size_t i = 0; i < items.size(); i++) {
        const Product &p = items[i].product;
        subtotal += p.price * p.quantity;
        if (!list.empty()) list += ",";
        list += itemJson(items[i]);
        if (best == -1 || p.price > items[best].product.price) best = (int)i;
    }

    int percent = discountFor(subtotal);
    double discount = subtotal * percent / 100.0;

    string top = "null";
    if (best != -1) {
        top = "{\"id\":" + to_string(items[best].id) +
              ",\"name\":\"" + jsonEscape(items[best].product.name) + "\"" +
              ",\"price\":" + num(items[best].product.price) + "}";
    }

    result = "{\"items\":[" + list + "]" +
             ",\"subtotal\":" + num(subtotal) +
             ",\"discountPercent\":" + to_string(percent) +
             ",\"discountAmount\":" + num(discount) +
             ",\"total\":" + num(subtotal - discount) +
             ",\"mostExpensive\":" + top + "}";
    return result.c_str();
}

// JSON: products whose name contains the text (ignores letter case)
EMSCRIPTEN_KEEPALIVE
const char *search_products(const char *rawQuery) {
    string q = toLower(trim(rawQuery ? rawQuery : ""));
    string list;
    for (const Item &it : items) {
        if (toLower(it.product.name).find(q) != string::npos) {
            if (!list.empty()) list += ",";
            list += itemJson(it);
        }
    }
    result = "{\"items\":[" + list + "]}";
    return result.c_str();
}

}  // extern "C"
