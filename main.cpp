#include <iostream>
#include <cstring>
#include <vector>
#include <queue>
#include <map>
#include <cassert>

struct huff_code {
  uint8_t val;
  uint32_t freq;
  std::string code;

  huff_code(uint8_t v = ' ', uint32_t f = 1, std::string &&c = "") 
    : val(v), freq(f), code(std::move(c)) {}
};

enum huff_type {
  INTERNAL, LEAF
};

struct huff_node {
  huff_node *left, *right;
  huff_code data;
  huff_type type;

  huff_node(huff_code d = huff_code(), 
            huff_node *l = nullptr, huff_node *r = nullptr, 
            huff_type t = LEAF) 
    : data(d), left(l), right(r), type(t) {}

  class Compare {
  public:
    bool operator()(huff_node *a, huff_node *b) {
      return a->data.freq > b->data.freq;
    }
  };

};

struct huffman {
  huff_node *root;
  uint32_t size;

  huffman() : root(nullptr), size(0) {}

  // for each leaf node, give a specific code
  // left: 0, right: 1
  void _build_tree(huff_node *p, std::string &prefix, uint8_t t) {
    if (p == nullptr) return;
    // skip the root node
    if (p != root) {
      p->data.code = prefix;
      if (t == 0) {p->data.code += "0"; }
      else {p->data.code += "1"; }
      if (p->type == LEAF) {
        return;
      }
    }
    _build_tree(p->left, p->data.code, 0);
    _build_tree(p->right, p->data.code, 1);
  }

  void build_tree() {
    _build_tree(root, root->data.code, 0);
  }

  void convert_to_vector(std::vector<huff_code> &v) {
    huff_node *p = root;
    std::queue<huff_node *> q;
    q.push(p);
    while (!q.empty()) {
      p = q.front();
      q.pop();
      if (p->type == LEAF) {
        v.push_back(p->data);
      } else {
        if (p->left != nullptr) q.push(p->left);
        if (p->right != nullptr) q.push(p->right);
      }
    }
  }

  // for debug
  void _print_tree(huff_node *p, int prefix) {
    char prefix_str[prefix + 1];
    for (int i = 0; i < prefix; i++) prefix_str[i] = ' ';
    prefix_str[prefix] = '\0';
    if (p == nullptr) return;
    printf("%s--", prefix_str);
    if (p->type == LEAF) {
      if (p->data.code == "") {
        printf("'%d'\n", p->data.val);
      } else {
        printf("[%s]'%d'\n", p->data.code.c_str(), p->data.val);
      }
    } else {
      printf("%u:\n", p->data.freq);
    }
    _print_tree(p->left, prefix + 2);
    _print_tree(p->right, prefix + 2);
  }
  void print_tree() {
    printf("huffman tree(%u)\n", size);
    _print_tree(root, 0);
  }
};

// read char from text file
std::string read_from(const std::string &filename) {
  FILE *fp = fopen(filename.c_str(), "r+");
  fseek(fp, 0L, SEEK_END);
  int size = ftell(fp);
  fseek(fp, 0L, SEEK_SET);
  char buffer[size + 1];
  fread(buffer, size, size, fp);
  buffer[size] = '\0';
  fclose(fp);
  return std::string(buffer);
}

// build huffman tree
void build_huffman(huffman &huffman, const std::string &contents) {
  // loop and count the freq of char
  std::map<uint8_t, huff_node*> huff_map;
  for (uint8_t ch : contents) {
    if (huff_map.find(ch) == huff_map.end()) {
      huff_code code(ch);
      huff_node *node = new huff_node(code);
      huff_map.insert({ch, node});
    } else {
      huff_map[ch]->data.freq++;
    }
  }
  // push all into priority queue
  std::priority_queue<huff_node *, std::vector<huff_node *>, huff_node::Compare> queue;
  for (auto &pair : huff_map) {
    queue.push(pair.second);
  }
  // get node nums
  uint32_t size = queue.size();
  // build huffman tree
  while (queue.size() >= 2) {
    auto first = queue.top(); queue.pop();
    auto second = queue.top(); queue.pop();
    huff_node *parent = new huff_node();
    parent->left = first;
    parent->right = second;
    parent->type = INTERNAL;
    parent->data.freq = first->data.freq + second->data.freq;
    queue.push(parent);
  }
  assert(queue.size() == 1);
  huff_node *root = queue.top();
  huffman.root = root;
  huffman.size = size;
  huffman.build_tree();
  // huffman.print_tree();
}

struct huff_info {
  uint8_t val;
  uint16_t freq;
  char code[16];
} __attribute__((__packed__));

struct huff_head {
  uint8_t magic[4];
  uint32_t size;
};

void write_head(std::vector<huff_code> &v, FILE *fp) {
  huff_head *head = new huff_head();
  memcpy(head->magic, ".HUF", 4);
  head->size = v.size();
  uint8_t *content = new uint8_t[sizeof(huff_head)];
  memcpy(content, head, sizeof(huff_head));
  fwrite(content, sizeof(huff_head), 1, fp);
  for (auto &val : v) {
    huff_info *info = new huff_info();
    info->freq = val.freq;
    assert(val.code.size() < 16);
    strncpy(info->code, val.code.c_str(), val.code.size());
    uint8_t *info_str = new uint8_t[sizeof(huff_info)];
    memcpy(info_str, (uint8_t *) info, sizeof(huff_info));
    fwrite(info_str, sizeof(huff_info), 1, fp);
  }
}

void write_to_file(const std::string &contents, std::vector<huff_code> &v, const std::string &filename) {
  FILE *fp = fopen(filename.c_str(), "w+");
  write_head(v, fp);
  std::string codes;
  std::map<uint8_t, huff_code> maps;
  for (auto val : v) {
    if (maps.find(val.val) == maps.end()) {
      maps.insert({val.val, val});
    }
  }
  // get the encoded 0/1 sequence
  for (auto ch : contents) codes += maps[ch].code;
  printf("code(%lu)\n", codes.size());
  for (int i = 0; i < codes.size(); i += 8) {
    std::string val = codes.substr(i, 8);
    uint8_t code = strtol(val.c_str(), nullptr, 2);
    fwrite(&code, sizeof(code), 1, fp);
  }
  fclose(fp);
}

int main(int argc, char *argv[]) {
  assert(argc == 2);
  huffman huffman;
  std::string contents = read_from(argv[1]);
  build_huffman(huffman, contents);
  std::vector<huff_code> v;
  huffman.convert_to_vector(v);
  write_to_file(contents, v, "a.huff");
  return 0;
}
