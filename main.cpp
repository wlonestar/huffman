#include <bitset>
#include <cassert>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <map>
#include <queue>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
using namespace std;

#define MAGIC 0x4655482e

struct huff_code {
  uint8_t val;
  uint32_t freq;
  string code;

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
  void _encode_tree(huff_node *p, string &prefix, uint8_t t) {
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

  void extract_maps(map<uint8_t, huff_code> &maps) {
    queue<huff_node *> q;
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

void maps_generator(const string &contents, map<uint8_t, huff_node *> &maps) {
  // map<uint8_t, huff_node*> maps;
  for (uint8_t ch: contents) {
    if (maps.find(ch) == maps.end()) {
      huff_code code(ch);
      huff_node *node = new huff_node(move(code));
      maps.insert({ch, node});
    } else {
      maps[ch]->data.freq++;
    }
  }
}

// build huffman tree
void build_tree(const string &contents, huff_tree &t) {
  // loop and count the freq of char
  map<uint8_t, huff_node *> maps;
  maps_generator(contents, maps);
  // push all into priority queue
  priority_queue<huff_node *, vector<huff_node *>, huff_node::Compare> q;
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
    huff_node *parent = new huff_node(move(code), left, right, INTERNAL);
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
void rebuild_tree(vector<huff_code> &v, huff_tree &t) {
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

int write_head(int fd, huff_tree &t, int length) {
  int head_size = sizeof(huff_head);
  huff_head head{
    .magic = MAGIC,
    .size = static_cast<uint16_t>(t.size),
    .last_length = static_cast<uint16_t>(length)};
  char *buffer = new char[head_size];
  memcpy(buffer, &head, head_size);
  return write(fd, buffer, head_size);
}
int write_entry(int fd, huff_tree &t, vector<huff_code> &v) {
  for (auto code: v) {
    int entry_size = sizeof(huff_entry);
    bitset<16> b(code.code.c_str());
    uint16_t t = b.to_ulong();
    huff_entry entry{
      .val = code.val,
      .length = static_cast<uint8_t>(code.code.size()),
      .code = t,
    };
    char *buffer = new char[entry_size];
    memcpy(buffer, &entry, entry_size);
    ssize_t ret = write(fd, buffer, entry_size);
  }
  return 0;
}
void write_contents(int fd, string &dst, huff_tree &t) {
  int i, times = dst.size() / 8;
  for (i = 0; i < times * 8; i += 8) {
    bitset<8> b(dst.substr(i, 8));
    uint8_t val = b.to_ulong();
    write(fd, &val, 1);
  }
  bitset<8> b(dst.substr(i, dst.size() - i));
  uint8_t val = b.to_ulong();
  write(fd, &val, 1);
}

int read_head(int fd, int *entry_size, int *last_length) {
  int head_size = sizeof(huff_head);
  huff_head head;
  int ret = read(fd, &head, head_size);
  if (head.magic != MAGIC) return -1;
  *entry_size = head.size;
  *last_length = head.last_length;
  return 0;
}
void convert_uint16_to_01_str(uint16_t code, int length, string &ret) {
  for (int i = 0; i < length; i++) {
    int val = (code & (1 << i)) >> i;
    ret.insert(ret.begin(), val == 0 ? '0' : '1');
  }
}

int read_entry(int fd, int entry_size, vector<huff_code> &v) {
  for (int i = 0; i < entry_size; i++) {
    int entry_size = sizeof(huff_entry);
    huff_entry entry;
    int ret = read(fd, &entry, entry_size);
    if (ret == -1) return -1;
    huff_code code(entry.val);
    string str;
    convert_uint16_to_01_str(entry.code, entry.length, str);
    code.code = str;
    v.push_back(code);
  }
  return 0;
}
void read_contents(int fd, string &contents, int file_size, int entry_size, int file_length) {
  int contents_size = file_size - sizeof(huff_head) - sizeof(huff_entry) * entry_size;
  char *buffer = new char[contents_size];
  int ret_sz = read(fd, buffer, contents_size);
  for (int i = 0; i < contents_size - 1; i++) {
    uint8_t ch = buffer[i];
    string tmp = "";
    for (int j = 7; j >= 0; j--) {
      int val = (ch & (1 << j)) >> j;
      tmp.insert(tmp.end(), val == 0 ? '0' : '1');
    }
    contents.append(tmp);
  }
  // special for the last
  uint8_t ch = buffer[contents_size - 1];
  string tmp = "";
  for (int i = 7; i >= 7 - file_length; i--) {
    int val = (ch & (1 << i)) >> i;
    tmp.insert(tmp.end(), val == 0 ? '0' : '1');
  }
  contents.append(tmp);
}
void decode(string &src, string &dst, huff_tree &t) {
  huff_node *p = t.root;
  for (uint8_t ch: src) {
    p = (ch == '0') ? p->left : p->right;
    if (p->type == LEAF) {
      dst.push_back(p->data.val);
      p = t.root;
    }
  }
}

/**
 * encode: read and write into huff file
 */

int read_text(const char *filename, string &contents) {
  int fd = open(filename, O_RDONLY);
  if (fd == -1) return -1;
  struct stat st;
  stat(filename, &st);
  int size = st.st_size;
  char *buffer = new char[size];
  read(fd, buffer, size);
  contents = string(buffer);
  close(fd);
  return 0;
}

int write_code(const char *filename, string &contents, huff_tree &t) {
  int fd = open(filename, O_CREAT | O_RDWR, 0644);
  if (fd == -1) return -1;
  map<uint8_t, huff_code> maps;
  t.extract_maps(maps);
  vector<huff_code> v;
  for (auto val: maps) v.push_back(val.second);
  string dst;
  for (auto ch: contents) dst.append(maps[ch].code);
  // byte, not bit size
  int last_length = dst.size() % 8;
  ssize_t ret = write_head(fd, t, last_length);
  write_entry(fd, t, v);
  int file_size = sizeof(huff_head) + sizeof(huff_entry) * t.size;
  write_contents(fd, dst, t);
  close(fd);
  return 0;
}

/**
 * decode: read from huff file and write into file
 */

int read_code(const char *filename, string &src, huff_tree &t) {
  int fd = open(filename, O_RDONLY);
  if (fd == -1) return -1;
  struct stat st;
  stat(filename, &st);
  int file_size = st.st_size;
  int entry_size;
  int last_length;
  int ret = read_head(fd, &entry_size, &last_length);
  if (ret == -1) return -1;
  cout << "entry_size:" << entry_size << ",last_length:" << last_length << "\n";
  vector<huff_code> v;
  read_entry(fd, entry_size, v);
  rebuild_tree(v, t);
  read_contents(fd, src, file_size, entry_size, last_length);
  return 0;
}

int write_text(const char *filename, string &src, huff_tree &t) {
  int fd = open(filename, O_CREAT | O_RDWR, 0644);
  if (fd == -1) return -1;
  string dst = "";
  decode(src, dst, t);
  int dst_size = dst.size();
  int ret = write(fd, dst.c_str(), dst_size);
  return 0;
}

/**
 * huffman <encode/decode> <input> <output>
 */
int main(int argc, char *argv[]) {
  assert(argc == 4);
  char *mode = argv[1];
  char *input_file = argv[2];
  char *output_file = argv[3];
  huff_tree t;
  string src = "";
  if (strcmp(mode, "encode") == 0) {
    int rd = read_text(input_file, src);
    if (rd == -1) return -1;
    build_tree(src, t);
    write_code(output_file, src, t);
  } else if (strcmp(mode, "decode") == 0) {
    int rd = read_code(input_file, src, t);
    if (rd == -1) return -1;
    write_text(output_file, src, t);
  } else {
    return -1;
  }
  return 0;
}
