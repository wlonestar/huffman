#include <bitset>
#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <queue>
#include <vector>
namespace fs = std::filesystem;

#define MAGIC 0x4655482e// huff file magic number: .HUF

struct huff_code {
  uint8_t val;
  uint32_t freq;
  std::string code;

  huff_code(uint8_t v = ' ', uint32_t f = 1) : val(v), freq(f) {}
};

enum huff_type {
  INTERNAL,
  LEAF
};

struct huff_node {
  huff_code data;
  huff_node *left, *right;
  huff_type type;

  huff_node(huff_code &&d = huff_code(),
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

struct huff_tree {
  huff_node *root;
  uint32_t size;

  huff_tree() : root(nullptr), size(0) {}

  // for each leaf node, give a specific code
  // left: 0, right: 1
  void _encode_tree(huff_node *p, std::string &prefix, uint8_t t) {
    if (p == nullptr) return;
    // skip the root node
    if (p != root) {
      p->data.code = prefix;
      p->data.code += (t == 0 ? "0" : "1");
      if (p->type == LEAF) return;
    }
    _encode_tree(p->left, p->data.code, 0);
    _encode_tree(p->right, p->data.code, 1);
  }
  void encode_tree() {
    _encode_tree(root, root->data.code, 0);
  }

  void extract_maps(std::map<uint8_t, huff_code> &maps) {
    std::queue<huff_node *> q;
    huff_node *p = root;
    q.push(p);
    while (!q.empty()) {
      p = q.front();
      q.pop();
      if (p->type == LEAF) maps.insert({p->data.val, p->data});
      if (p->left != nullptr) q.push(p->left);
      if (p->right != nullptr) q.push(p->right);
    }
  }

  // for debug
  void __print_tree(huff_node *p, int prefix) {
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
    __print_tree(p->left, prefix + 2);
    __print_tree(p->right, prefix + 2);
  }
  void _print_tree() {
    printf("huffman tree(%u)\n", size);
    __print_tree(root, 0);
  }
};

void maps_generator(const std::string &src, std::map<uint8_t, huff_node *> &maps) {
  // map<uint8_t, huff_node*> maps;
  for (uint8_t ch: src) {
    if (maps.find(ch) == maps.end()) {
      huff_code code(ch);
      huff_node *node = new huff_node(std::move(code));
      maps.insert({ch, node});
    } else {
      maps[ch]->data.freq++;
    }
  }
}

// build huffman tree
void build_tree(const std::string &src, huff_tree &t) {
  // loop and count the freq of char
  std::map<uint8_t, huff_node *> maps;
  maps_generator(src, maps);
  // push all into priority queue
  std::priority_queue<huff_node *, std::vector<huff_node *>, huff_node::Compare> q;
  for (auto &pair: maps) q.push(pair.second);
  // get node nums
  uint32_t size = q.size();
  // build huffman tree
  while (q.size() >= 2) {
    huff_node *left = q.top();
    q.pop();
    huff_node *right = q.top();
    q.pop();
    huff_code code(' ', left->data.freq + right->data.freq);
    huff_node *parent = new huff_node(std::move(code), left, right, INTERNAL);
    q.push(parent);
  }
  assert(q.size() == 1);
  huff_node *root = q.top();
  t.root = root;
  t.size = size;
  t.encode_tree();
}

/**
 * build huff tree based on encoded huff code
 */
void rebuild_tree(std::vector<huff_code> &v, huff_tree &t) {
  t.root = new huff_node();
  t.root->data.code = "";
  huff_node *p;
  for (auto &item: v) {
    p = t.root;
    for (int i = 0; i < item.code.size(); i++) {
      char ch = item.code[i];
      huff_code code(item.val);
      if (ch == '0') {
        if (p->left == nullptr) {
          huff_node *left = new huff_node();
          left->type = (i == item.code.size() - 1) ? LEAF : INTERNAL;
          p->left = left;
        }
        code.code = p->data.code + "0";
        p->left->data = code;
        p = p->left;
      } else if (ch == '1') {
        if (p->right == nullptr) {
          huff_node *right = new huff_node();
          right->type = (i == item.code.size() - 1) ? LEAF : INTERNAL;
          p->right = right;
        }
        code.code = p->data.code + "1";
        p->right->data = code;
        p = p->right;
      }
    }
  }
}

/**
 * file head: 8B
 */
struct huff_head {
  uint32_t magic;
  uint16_t size;
  uint16_t last_length;
} __attribute__((__packed__));

/**
 * huffman entry: 3B
 */
struct huff_entry {
  uint8_t val;
  uint8_t length;
  uint16_t code;
} __attribute__((__packed__));

class huffman_coder_base {
protected:
  huff_tree t;
  std::string src, dst;
  char *input_file, *output_file;
  size_t file_size;
  std::ifstream in;
  std::ofstream out;
  std::vector<huff_code> v;

public:
  huffman_coder_base(char *input, char *output)
      : input_file(input), output_file(output) {
    src = dst = "";
  }

  ~huffman_coder_base() {
    in.close();
    out.close();
  }

  virtual int read_init() {
    if (!fs::exists(input_file)) return -1;
    fs::path p = input_file;
    file_size = fs::file_size(p);
    in = std::ifstream(p, std::ios::in | std::ios::binary);
    if (!in) return -1;
    return 0;
  }

  virtual int write_init() {
    if (!fs::exists(output_file)) {
      fs::create_directories(fs::current_path());
      out = std::ofstream(output_file);
    }
    out = std::ofstream(output_file, std::ios::trunc);
    if (!out.is_open()) return -1;
    return 0;
  }

  virtual int read() = 0;
  virtual int write() = 0;
};

class huffman_encoder : protected huffman_coder_base {
private:
  size_t last_length;
  std::map<uint8_t, huff_code> maps;

private:
  int write_head() {
    int head_size = sizeof(huff_head);
    huff_head head{
      .magic = MAGIC,
      .size = static_cast<uint16_t>(t.size),
      .last_length = static_cast<uint16_t>(last_length)};
    char *buffer = new char[head_size];
    memcpy(buffer, &head, head_size);
    out.write(buffer, head_size);
    return 0;
  }

  int write_entry() {
    for (auto code: v) {
      int entry_size = sizeof(huff_entry);
      std::bitset<16> b(code.code.c_str());
      uint16_t t = b.to_ulong();
      huff_entry entry{
        .val = code.val,
        .length = static_cast<uint8_t>(code.code.size()),
        .code = t,
      };
      char *buffer = new char[entry_size];
      memcpy(buffer, &entry, entry_size);
      out.write(buffer, entry_size);
    }
    return 0;
  }

  void write_contents() {
    int i, times = dst.size() / 8;
    for (i = 0; i < times * 8; i += 8) {
      std::bitset<8> b(dst.substr(i, 8));
      uint8_t val = b.to_ulong();
      char *buffer = new char[1];
      memcpy(buffer, &val, 1);
      out.write(buffer, 1);
    }
    std::bitset<8> b(dst.substr(i, dst.size() - i));
    uint8_t val = b.to_ulong();
    char *buffer = new char[1];
    memcpy(buffer, &val, 1);
    out.write(buffer, 1);
  }

public:
  huffman_encoder(char *input, char *output) : huffman_coder_base(input, output) {}

  int read() override {
    if (read_init() == -1) return -1;
    src.resize(file_size);
    in.read(src.data(), file_size);
    return 0;
  }

  int write() override {
    if (write_init() == -1) return -1;

    build_tree(src, t);
    t.extract_maps(maps);
    for (auto val: maps) v.push_back(val.second);
    for (auto ch: src) dst.append(maps[ch].code);
    last_length = dst.size() % 8;

    write_head();
    write_entry();
    write_contents();
    return 0;
  }
};

class huffman_decoder : protected huffman_coder_base {
private:
  size_t entry_size;
  size_t last_length;

private:
  int read_head() {
    int head_size = sizeof(huff_head);
    huff_head head;
    char *buffer = new char[head_size];
    in.read(buffer, head_size);
    memcpy(&head, buffer, head_size);
    if (head.magic != MAGIC) return -1;
    entry_size = head.size;
    last_length = head.last_length;
    return 0;
  }

  void convert_uint16_to_01_str(uint16_t code, int length, std::string &ret) {
    for (int i = 0; i < length; i++) {
      int val = (code & (1 << i)) >> i;
      ret.insert(ret.begin(), val == 0 ? '0' : '1');
    }
  }

  int read_entry() {
    for (int i = 0; i < entry_size; i++) {
      int entry_size = sizeof(huff_entry);
      huff_entry entry;
      char *buffer = new char[entry_size];
      in.read(buffer, entry_size);
      memcpy(&entry, buffer, entry_size);
      huff_code code(entry.val);
      std::string str;
      convert_uint16_to_01_str(entry.code, entry.length, str);
      code.code = str;
      v.push_back(code);
    }
    return 0;
  }

  void read_contents() {
    int contents_size = file_size - sizeof(huff_head) - sizeof(huff_entry) * entry_size;
    char *buffer = new char[contents_size];
    in.read(buffer, contents_size);
    for (int i = 0; i < contents_size - 1; i++) {
      uint8_t ch = buffer[i];
      std::string tmp = "";
      for (int j = 7; j >= 0; j--) {
        int val = (ch & (1 << j)) >> j;
        tmp.insert(tmp.end(), val == 0 ? '0' : '1');
      }
      src.append(tmp);
    }
    // special for the last
    uint8_t ch = buffer[contents_size - 1];
    std::string tmp = "";
    for (int i = 7; i >= 7 - last_length; i--) {
      int val = (ch & (1 << i)) >> i;
      tmp.insert(tmp.end(), val == 0 ? '0' : '1');
    }
    src.append(tmp);
  }

  void decode() {
    huff_node *p = t.root;
    for (uint8_t ch: src) {
      p = (ch == '0') ? p->left : p->right;
      if (p->type == LEAF) {
        dst.push_back(p->data.val);
        p = t.root;
      }
    }
  }

public:
  huffman_decoder(char *input, char *output) : huffman_coder_base(input, output) {}

  int read() override {
    if (read_init() == -1) return -1;
    read_head();
    read_entry();
    rebuild_tree(v, t);
    read_contents();
    return 0;
  }

  int write() override {
    if (write_init() == -1) return -1;
    decode();
    out.write(dst.c_str(), dst.size());
    return 0;
  }
};

/**
 * huffman <encode/decode> <input> <output>
 */
int main(int argc, char *argv[]) {
  assert(argc == 4);
  char *mode = argv[1];
  char *input_file = argv[2];
  char *output_file = argv[3];
  if (strcmp(mode, "encode") == 0) {
    huffman_encoder encoder(input_file, output_file);
    encoder.read();
    encoder.write();
  } else if (strcmp(mode, "decode") == 0) {
    huffman_decoder decoder(input_file, output_file);
    decoder.read();
    decoder.write();
  } else {
    return -1;
  }
  return 0;
}
