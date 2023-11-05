#ifndef huffman_hpp
#define huffman_hpp

#include <bitset>
#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <queue>
#include <vector>

namespace huffman {

// print infomation with color
#define LOG_ERROR "\033[31m"
#define LOG_WARN "\033[35m"
#define LOG_DEBUG "\033[34m"
#define LOG_INFO "\033[32m"
#define log_base(level, fmt, ...) \
  fprintf(stderr, level "huffman: " fmt "\033[0m\n", ##__VA_ARGS__)
#define error(fmt, ...) log_base(LOG_ERROR, fmt, ##__VA_ARGS__)
#define warn(fmt, ...) log_base(LOG_WARN, fmt, ##__VA_ARGS__)
#define debug(fmt, ...) log_base(LOG_DEBUG, fmt, ##__VA_ARGS__)
#define info(fmt, ...) log_base(LOG_INFO, fmt, ##__VA_ARGS__)

#define HUFF_MAGIC 0x4655482e// huff file magic number: .HUF

namespace fs = std::filesystem;

struct huff_code {
  uint8_t val;
  uint32_t freq;
  std::string code;

  huff_code(uint8_t v = ' ', uint32_t f = 1) : val(v), freq(f) { code = ""; }
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

class huff_tree {
private:
  huff_node *_root;
  uint32_t _size;
  std::map<uint8_t, huff_node *> _map;
  std::vector<huff_code> _list;

private:
  void __encode_node(huff_node *p, std::string &prefix, uint8_t t) {
    if (p == nullptr) return;
    // skip the root node
    if (p != _root) {
      p->data.code = prefix;
      p->data.code += (t == 0 ? "0" : "1");// left: 0, right: 1
      if (p->type == LEAF) return;
    }
    __encode_node(p->left, p->data.code, 0);
    __encode_node(p->right, p->data.code, 1);
  }

  /**
   * give each leaf node a specific code
   */
  void _encode_node() {
    __encode_node(_root, _root->data.code, 0);
  }

  /**
   * generator maps: loop and count the freq of char
   */
  void _maps_generator(const std::string &src) {
    for (uint8_t ch: src) {
      if (_map.find(ch) == _map.end()) {
        huff_code code(ch);
        huff_node *node = new huff_node(std::move(code));
        _map.insert({ch, std::move(node)});
      } else {
        _map[ch]->data.freq++;
      }
    }
  }

  /**
   * build huffman tree based text char freq
   */
  void _build(const std::string &src) {
    _maps_generator(src);
    // push all into priority queue
    using pq = std::priority_queue<huff_node *,
                                   std::vector<huff_node *>,
                                   huff_node::Compare>;
    pq q;
    for (auto &pair: _map) q.push(pair.second);
    uint32_t node_size = q.size();// get node size
    while (q.size() >= 2) {
      huff_node *left = q.top();
      q.pop();
      huff_node *right = q.top();
      q.pop();
      huff_code code(' ', left->data.freq + right->data.freq);
      huff_node *p = new huff_node(std::move(code), left, right, INTERNAL);
      q.push(p);
    }
    assert(q.size() == 1);
    _root = q.top();
    _size = node_size;
    _encode_node();
  }

public:
  huff_tree() : _root(nullptr), _size(0) {}

  uint32_t size() const { return _size; }

  std::vector<huff_code> &get_list() { return _list; }
  void set_list(std::vector<huff_code> &&list) { _list = std::move(list); }

  /**
   * convert text to 0/1 sequence
   */
  void encode(const std::string &src, std::string &dst) {
    _build(src);
    // store in vector
    for (auto val: _map) _list.push_back(val.second->data);
    // convert to 0/1 sequence by map
    for (auto ch: src) dst.append(_map[ch]->data.code);
  }

  void decode(const std::string &src, std::string &dst) {
    huff_node *p = _root;
    for (uint8_t ch: src) {
      p = (ch == '0') ? p->left : p->right;
      if (p->type == LEAF) {
        dst.push_back(p->data.val);
        p = _root;
      }
    }
  }

  /**
   * build huff tree based on encoded huff code
   */
  void build() {
    _root = new huff_node();
    huff_node *p;
    for (auto &item: _list) {
      p = _root;
      for (int i = 0; i < item.code.size(); i++) {
        char ch = item.code[i];
        huff_code code(item.val);
        huff_type type = (i == item.code.size() - 1) ? LEAF : INTERNAL;
        huff_node *child = new huff_node(huff_code(), nullptr, nullptr, type);
        if (ch == '0') {
          if (p->left == nullptr) p->left = child;
          code.code = p->data.code + "0";
          p->left->data = std::move(code);
          p = p->left;
        } else if (ch == '1') {
          if (p->right == nullptr) p->right = child;
          code.code = p->data.code + "1";
          p->right->data = std::move(code);
          p = p->right;
        }
      }
    }
  }

public:
  // for debug
  void __print_tree(huff_node *p, int prefix) {
    std::string prefix_str;
    for (int i = 0; i < prefix; i++) prefix_str.push_back(' ');
    if (p == nullptr) return;
    printf("%s--", prefix_str.c_str());
    if (p->type == LEAF) {
      printf("[%d]", p->data.val);
      if (p->data.code != "") printf("(%s)", p->data.code.c_str());
      printf("\n");
    } else {
      printf("%u:\n", p->data.freq);
    }
    __print_tree(p->left, prefix + 2);
    __print_tree(p->right, prefix + 2);
  }

  void _print_tree() {
    printf("huffman tree(%u)\n", _size);
    __print_tree(_root, 0);
  }
};

/**
 * file head: 8B
 */
struct huff_head {
  uint32_t magic;
  uint16_t size;
  uint16_t last_length;
} __attribute__((__packed__));

void huff_head_init(huff_head &h, uint16_t size,
                    uint16_t last_length) {
  h.magic = HUFF_MAGIC;
  h.size = size;
  h.last_length = last_length;
}

/**
 * huffman entry: 4B
 */
struct huff_entry {
  uint8_t val;
  uint8_t length;
  uint16_t code;
} __attribute__((__packed__));

void huff_entry_init(huff_entry &e, uint8_t val,
                     uint8_t length, uint16_t code) {
  e.val = val;
  e.length = length;
  e.code = code;
}

class huffman_coder_base {
protected:
  huff_tree t;
  std::string src, dst;
  std::string input_file, output_file;
  size_t file_size, last_length;
  std::ifstream in;
  std::ofstream out;

public:
  huffman_coder_base(std::string &&input, std::string &&output)
      : input_file(std::move(input)), output_file(std::move(output)) {
    src = dst = "";
  }

  ~huffman_coder_base() {
    in.close();
    out.close();
  }

  virtual void read_init() {
    if (!fs::exists(input_file)) {
      error("%s file not exists.", input_file.c_str());
      exit(EXIT_FAILURE);
    }
    fs::path p = input_file;
    file_size = fs::file_size(p);
    in = std::ifstream(p, std::ios::in | std::ios::binary);
    if (!in) {
      error("can not access to file %s.", input_file.c_str());
      exit(EXIT_FAILURE);
    }
  }

  virtual void write_init() {
    if (!fs::exists(output_file)) {
      fs::create_directories(fs::current_path());
      out = std::ofstream(output_file);
    }
    out = std::ofstream(output_file, std::ios::trunc);
    if (!out.is_open()) {
      error("can not access to file %s.", output_file.c_str());
      exit(EXIT_FAILURE);
    }
  }

  virtual void read() = 0;
  virtual void write() = 0;
};

class huffman_encoder : protected huffman_coder_base {
private:
  void write_head() {
    int head_size = sizeof(huff_head);
    huff_head head;
    huff_head_init(head, t.size(), last_length);
    char *buffer = new char[head_size];
    memcpy(buffer, &head, head_size);
    out.write(buffer, head_size);
    delete [] buffer;
  }

  void write_entry() {
    for (auto code: t.get_list()) {
      int entry_size = sizeof(huff_entry);
      std::bitset<16> b(code.code.c_str());
      uint16_t t = b.to_ulong();
      huff_entry entry;
      huff_entry_init(entry, code.val, (uint8_t) code.code.size(), t);
      char *buffer = new char[entry_size];
      memcpy(buffer, &entry, entry_size);
      out.write(buffer, entry_size);
      delete [] buffer;
    }
  }

  void write_contents() {
    int i, times = dst.size() / 8;
    for (i = 0; i < times * 8; i += 8) {
      std::bitset<8> b(dst.substr(i, 8));
      uint8_t val = b.to_ulong();
      char buffer = static_cast<char>(val);
      out.write(&buffer, 1);
    }
    /**
     * deal with last byte less 8 bit: append 0 bit
     * eg: [01]000000
     */
    std::string last = dst.substr(i, dst.size() - i);
    for (int k = 0; k < 8 - (dst.size() - i); k++) last.push_back('0');
    std::bitset<8> b(last);
    uint8_t val = b.to_ulong();
    char buffer = static_cast<char>(val);
    out.write(&buffer, 1);
  }

public:
  huffman_encoder(std::string &&input, std::string &&output)
      : huffman_coder_base(std::move(input), std::move(output)) {}

  void read() override {
    read_init();
    src.resize(file_size);
    in.read(src.data(), file_size);
  }

  void write() override {
    write_init();
    t.encode(src, dst);
    last_length = dst.size() % 8;
    write_head();
    write_entry();
    write_contents();
  }

  void encode() {
    read();
    write();
  }
};

class huffman_decoder : protected huffman_coder_base {
private:
  size_t entry_size;

private:
  int read_head() {
    int head_size = sizeof(huff_head);
    huff_head head;
    char *buffer = new char[head_size];
    in.read(buffer, head_size);
    memcpy(&head, buffer, head_size);
    if (head.magic != HUFF_MAGIC) return -1;
    entry_size = head.size;
    last_length = head.last_length;
    return 0;
  }

  void convert_uint16_to_01_str(uint16_t val, int len, std::string &dst) {
    for (int i = len - 1; i >= 0; i--) {
      int bit = (val & (1 << i)) >> i;
      dst.push_back(bit == 0 ? '0' : '1');
    }
  }

  int read_entry() {
    std::vector<huff_code> list;
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
      list.push_back(code);
    }
    t.set_list(std::move(list));
    return 0;
  }

  void read_contents() {
    int cont_bytes =
      file_size - sizeof(huff_head) - sizeof(huff_entry) * entry_size;
    char *buffer = new char[cont_bytes];
    in.read(buffer, cont_bytes);
    for (int i = 0; i < cont_bytes - 1; i++) {
      uint8_t ch = buffer[i];
      std::string tmp = "";
      for (int j = 7; j >= 0; j--) {
        int val = (ch & (1 << j)) >> j;
        tmp.push_back(val == 0 ? '0' : '1');
      }
      src.append(tmp);
    }
    // special for the last
    std::string tmp = "";
    int end = (last_length == 0) ? -1 : (7 - last_length);
    for (int i = 7; i > end; i--) {
      int val = (buffer[cont_bytes - 1] & (1 << i)) >> i;
      tmp.push_back(val == 0 ? '0' : '1');
    }
    src.append(tmp);
  }

public:
  huffman_decoder(std::string &&input, std::string &&output)
      : huffman_coder_base(std::move(input), std::move(output)) {}

  void read() override {
    read_init();
    read_head();
    read_entry();
    t.build();
    read_contents();
  }

  void write() override {
    write_init();
    t.decode(src, dst);
    out.write(dst.c_str(), dst.size());
  }

  void decode() {
    read();
    write();
  }
};

}// namespace huffman

#endif
