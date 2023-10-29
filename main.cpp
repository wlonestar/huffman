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
 * file head: 8B
 */
struct huff_head {
  uint32_t magic;
  uint32_t size;
} __attribute__((__packed__));

/**
 * huffman entry: 3B
 */
struct huff_entry {
  uint8_t val;
  uint8_t length;
  uint16_t code;
} __attribute__((__packed__));

int write_head(int fd, huff_tree &t) {
  int head_size = sizeof(huff_head);
  huff_head head{.magic = MAGIC, .size = t.size};
  char *buffer = new char[head_size];
  memcpy(buffer, &head, head_size);
  printf("%x %d\n", head.magic, head.size);
  return write(fd, buffer, head_size);
}
int write_entry(int fd, string &contents, huff_tree &t, map<uint8_t, huff_code> &maps) {
  vector<huff_code> v;
  for (auto val: maps) v.push_back(val.second);
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
    // printf("write %zd size entry: %16b\n", ret, entry.code);
  }
  return 0;
}
void write_contents(int fd, string &contents, huff_tree &t, map<uint8_t, huff_code> &maps) {
  string bits;
  for (auto ch: contents) bits.append(maps[ch].code);
  // cout << bits << "\n" << bits.size() << "\n";
  for (int i = 0; i < bits.size(); i += 8) {
    bitset<16> b(bits.substr(i, 8));
    uint8_t val = b.to_ulong();
    ssize_t ret = write(fd, &val, 1);
    // printf("write %zd byte\n", ret);
  }
}

int read_head(int fd, int *entry_size) {
  int head_size = sizeof(huff_head);
  huff_head head;
  int ret = read(fd, &head, head_size);
  if (head.magic != MAGIC) return -1;
  *entry_size = head.size;
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
    printf("%c-%s\n", code.val, code.code.c_str());
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
  int fd = open(filename, O_CREAT | O_RDWR);
  if (fd == -1) return -1;
  map<uint8_t, huff_code> maps;
  t.extract_maps(maps);
  ssize_t ret = write_head(fd, t);
  // cout << "write " << ret << " huff file head\n";
  write_entry(fd, contents, t, maps);
  int file_size = sizeof(huff_head) + sizeof(huff_entry) * t.size;
  // cout << file_size << "\n";
  write_contents(fd, contents, t, maps);
  return 0;
}

/**
 * decode: read from huff file and write into file
 */

int read_code(const char *filename, string &contents) {
  int fd = open(filename, O_RDONLY);
  if (fd == -1) return -1;
  struct stat st;
  stat(filename, &st);
  int size = st.st_size;
  int entry_size;
  int ret = read_head(fd, &entry_size);
  if (ret == -1) return -1;
  vector<huff_code> v;
  read_entry(fd, entry_size, v);
  /**
   * TODO:
   * 
   * build huff tree based encoded huff char
   */

  return 0;
}

void write_text(const char *filename, string &contents, huff_tree &t) {
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
  string contents;
  if (strcmp(mode, "encode") == 0) {
    int rd = read_text(input_file, contents);
    if (rd == -1) return -1;
    build_tree(contents, t);
    write_code(output_file, contents, t);
  } else if (strcmp(mode, "decode") == 0) {
    int rd = read_code(input_file, contents);
    if (rd == -1) return -1;

  } else {
    return -1;
  }
  return 0;
}
