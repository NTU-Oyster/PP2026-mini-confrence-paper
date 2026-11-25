// Text format for HULH decision states, one per line:
//   id street history hole board
// history is a string over {f,c,r} from the start of the hand ("-" if empty);
// cards are two characters (rank, suit), board "-" preflop. Only information
// visible to the player to act is stored; hidden cards are resampled by the search.
#pragma once
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "game/hulh.hpp"

namespace pm {

struct DecisionState {
    int id = 0;
    std::string history;
    Hulh state;
};

inline std::string card_str(uint8_t c) {
    return std::string() + "23456789TJQKA"[c >> 2] + "cdhs"[c & 3];
}
inline uint8_t parse_card(const char* s) {
    static const char* R = "23456789TJQKA";
    static const char* S = "cdhs";
    const char* r = std::strchr(R, s[0]);
    const char* u = std::strchr(S, s[1]);
    if (!s[0] || !s[1] || !r || !u) throw std::runtime_error("bad card");
    return static_cast<uint8_t>((r - R) * 4 + (u - S));
}
inline std::string cards_str(const uint8_t* c, int n) {
    std::string out;
    for (int i = 0; i < n; ++i) out += card_str(c[i]);
    return out.empty() ? "-" : out;
}

inline std::string format_state(const DecisionState& d) {
    const Hulh& s = d.state;
    std::ostringstream o;
    o << d.id << ' ' << int(s.street) << ' ' << (d.history.empty() ? "-" : d.history) << ' '
      << cards_str(s.hole[s.player()], 2) << ' '
      << cards_str(s.board, Hulh::visible_board(s.street));
    return o.str();
}

inline DecisionState parse_state(const std::string& line) {
    std::istringstream in(line);
    DecisionState d;
    int street;
    std::string hole, board;
    if (!(in >> d.id >> street >> d.history >> hole >> board))
        throw std::runtime_error("bad state line: " + line);
    if (d.history == "-") d.history.clear();
    for (char ch : d.history) {
        const int a = ch == 'f' ? kFold : ch == 'c' ? kCall : kRaise;
        if (d.state.terminal() || !(d.state.legal_mask() >> a & 1))
            throw std::runtime_error("illegal history: " + line);
        d.state.apply(a);
    }
    Hulh& s = d.state;
    if (s.terminal() || s.street != street || hole.size() != 4)
        throw std::runtime_error("inconsistent state: " + line);
    for (int i = 0; i < 2; ++i) s.hole[s.player()][i] = parse_card(&hole[2 * i]);
    const int nb = Hulh::visible_board(street);
    if (nb && board.size() != static_cast<size_t>(2 * nb))
        throw std::runtime_error("bad board: " + line);
    for (int i = 0; i < nb; ++i) s.board[i] = parse_card(&board[2 * i]);
    return d;
}

inline std::vector<DecisionState> load_states(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("cannot open " + path);
    std::vector<DecisionState> v;
    for (std::string line; std::getline(f, line);)
        if (!line.empty() && line[0] != '#') v.push_back(parse_state(line));
    return v;
}

}  // namespace pm
