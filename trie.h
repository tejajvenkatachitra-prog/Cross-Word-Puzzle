#pragma once
#include <string>
#include <vector>
#include <memory>
#include <array>
#include <cctype>
struct TrieNode {
    std::array<std::unique_ptr<TrieNode>, 26> children;
    bool isWord = false;
};

class Trie {
public:
    Trie() : root_(std::make_unique<TrieNode>()) {}

    void insert(const std::string& word) {
        TrieNode* cur = root_.get();
        for (char ch : word) {
            int idx = charIndex(ch);
            if (idx < 0) return; // skip non a-z words defensively
            if (!cur->children[idx]) cur->children[idx] = std::make_unique<TrieNode>();
            cur = cur->children[idx].get();
        }
        cur->isWord = true;
    }
    // Exact lookup: is this fully-spelled word in the dictionary?
    bool contains(const std::string& word) const {
        const TrieNode* cur = root_.get();
        for (char ch : word) {
            int idx = charIndex(ch);
            if (idx < 0 || !cur->children[idx]) return false;
            cur = cur->children[idx].get();
        }
        return cur->isWord;
    }
    std::vector<std::string> matchPattern(const std::string& pattern) const {
        std::vector<std::string> results;
        std::string buffer(pattern.size(), ' ');
        search(root_.get(), pattern, 0, buffer, results);
        return results;
    }

private:
    std::unique_ptr<TrieNode> root_;

    static int charIndex(char ch) {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        if (ch < 'A' || ch > 'Z') return -1;
        return ch - 'A';
    }
    static void search(TrieNode* node, const std::string& pattern, size_t pos,
                        std::string& buffer, std::vector<std::string>& results) {
        if (!node) return;
        if (pos == pattern.size()) {
            if (node->isWord) results.push_back(buffer);
            return;
        }

        char ch = pattern[pos];
        if (ch == '.' || ch == '_' || ch == '-') {
            for (int i = 0; i < 26; i++) {
                if (node->children[i]) {
                    buffer[pos] = static_cast<char>('A' + i);
                    search(node->children[i].get(), pattern, pos + 1, buffer, results);
                }
            }
        } else {
            int idx = charIndex(ch);
            if (idx >= 0 && node->children[idx]) {
                buffer[pos] = static_cast<char>('A' + idx);
                search(node->children[idx].get(), pattern, pos + 1, buffer, results);
            }
        }
    }
};
