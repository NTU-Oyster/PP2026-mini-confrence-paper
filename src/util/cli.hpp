// --key value command-line parsing.
#pragma once
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace pm {

class Cli {
public:
    Cli(int argc, char** argv) {
        for (int i = 1; i < argc; i += 2) {
            const std::string k = argv[i];
            if (k.rfind("--", 0) != 0 || i + 1 >= argc)
                throw std::runtime_error("expected --key value, got " + k);
            kv_[k.substr(2)] = argv[i + 1];
        }
    }
    std::string str(const std::string& k, const std::string& d) const {
        auto it = kv_.find(k);
        return it == kv_.end() ? d : it->second;
    }
    double num(const std::string& k, double d) const {
        auto it = kv_.find(k);
        return it == kv_.end() ? d : std::stod(it->second);
    }
    std::vector<std::string> list(const std::string& k, const std::string& d) const {
        std::vector<std::string> v;
        std::istringstream in(str(k, d));
        for (std::string x; std::getline(in, x, ',');) v.push_back(x);
        return v;
    }

private:
    std::map<std::string, std::string> kv_;
};

}  // namespace pm
