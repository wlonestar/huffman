#include <iostream>
#include <queue>
#include <map>
#include <vector>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <bitset>

/**
 * string -> tokens -> huffman tree -> encode
 */

struct Token {
  uint8_t val;
  uint64_t freq;
  std::string code;

  Token() : val(' '), freq(0), code("") {}
  Token(uint8_t _val, uint64_t _freq) : val(_val), freq(_freq) {}
};

enum NodeType {
  INTERNAL,
  LEAF,
};

struct HuffNode {
  HuffNode *left;
  HuffNode *right;
  NodeType type;
  Token token;

  HuffNode(Token _token) : token(_token), type(LEAF), left(nullptr), right(nullptr) {}
  HuffNode(HuffNode *_left, HuffNode *_right) : token(), type(INTERNAL), left(_left), right(_right) {}

  class Compare {
  public:
    bool operator()(HuffNode *lhs, HuffNode *rhs) {
      return lhs->token.freq > rhs->token.freq;
    }
  };

};

struct HuffTree {
  HuffNode *root;
  uint64_t size;

  HuffTree(uint64_t _size) : size(_size) {}

  void addCode(std::string str, HuffNode *p) {
    if (p != nullptr) {
      if (p->type == LEAF) {
        p->token.code = str;
      }
      addCode(str + "0", p->left);
      addCode(str + "1", p->right);
    }
  }
  void addCode() {
    addCode("0", root->left);
    addCode("1", root->right);
  }

  void _getTokens(std::map<uint8_t, Token> &tokens, HuffNode *p) {
    if (p != nullptr) {
      if (p->type == LEAF) {
        tokens.insert({p->token.val, p->token});
        // tokens.push_back(p->token);
      }
      _getTokens(tokens, p->left);
      _getTokens(tokens, p->right);
    }
  }

  std::map<uint8_t, Token> getTokens() {
    std::map<uint8_t, Token> tokens;
    _getTokens(tokens, root->left);
    _getTokens(tokens, root->right);
    return tokens;
  }

  // for debug
  void print(HuffNode *p, int prefix) {
    char prefix_str[prefix];
    for (int i = 0; i < prefix; i++) {
      prefix_str[i] = ' ';
    }
    prefix_str[prefix] = '\0';
    if (p == nullptr) {
      return;
    }
    std::cout << prefix_str;
    std::cout << "--";
    if (p->type == LEAF) {
      printf("['%d':%ld:%s]\n", p->token.val, p->token.freq, p->token.code.c_str());
    } else {
      std::cout << p->token.freq << "\n";
    }
    print(p->left, prefix + 2);
    print(p->right, prefix + 2);
  }
  void print() {
    printf("huffman tree has node %ld\n", size);
    print(root, 0);
  }
};

void createHuffTree(std::map<uint8_t, Token> &tokens) {
  std::priority_queue<HuffNode *, std::vector<HuffNode*>, HuffNode::Compare> q;
  for (auto it = tokens.begin(); it != tokens.end(); ++it) {
    auto *node = new HuffNode(it->second);
    q.push(node);
  }

  HuffTree tree(q.size() * 2 - 1);
  while (q.size() > 1) {
    auto left = q.top();
    q.pop();
    auto right = q.top();
    q.pop();
    auto internal = new HuffNode(left, right);
    internal->token.freq = left->token.freq + right->token.freq;
    q.push(internal);
  }
  tree.root = q.top();
  q.pop(); // now q is empty
  assert(q.empty() == true);
  tree.addCode();
  tree.print();

  tokens = tree.getTokens();

}

std::string readFile(std::string &filename) {
  namespace fs = std::filesystem;
  auto path = fs::absolute(filename);
  if (!fs::exists(path)) {
    fprintf(stderr, "toyc: error: no such file or directory: '%s'",
            filename.c_str());
    exit(EXIT_FAILURE);
  }

  std::ifstream ifs(filename.c_str(),
                    std::ios::in | std::ios::binary | std::ios::ate);
  std::ifstream::pos_type fileSize = ifs.tellg();
  ifs.seekg(0, std::ios::beg);
  std::vector<char> bytes(fileSize);
  ifs.read(bytes.data(), fileSize);
  std::string content(bytes.data(), fileSize);
  return content;
}

void encode() {
  std::string test_filename = "b.txt";
  std::string content = readFile(test_filename);

  std::map<uint8_t, Token> maps;
  for (int i = 0; i < content.size(); i++) {
    uint8_t val = content[i];
    if (maps.contains(val) == true) {
      maps[val].freq += 1;
    } else {
      Token token(val, 0);
      maps.insert({val, token});
    }
  }

  createHuffTree(maps);

  std::string result = "";
  for (int i = 0; i < content.size(); i++) {
    uint8_t val = content[i];
    result += maps[val].code;
  }
  std::cout << result.size() << "\n";

  std::ofstream outfile("a.huff", std::ofstream::binary);
  int n = 8;
  for (int i = 0; i < result.size(); i+=8) {
    std::bitset<8> bits(result.substr(i, 8));
    uint8_t binary_value = bits.to_ulong();
    outfile.write((const char *)&binary_value, sizeof(uint8_t));
  }
  
  outfile.close();
}

int main() {
  encode();
  return 0;
}
