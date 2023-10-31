#include "huffman.hpp"

/**
 * huffman <encode/decode> <input> <output>
 */
int main(int argc, char *argv[]) {
  assert(argc == 4);
  char *mode = argv[1];
  char *input_file = argv[2];
  char *output_file = argv[3];
  if (strcmp(mode, "encode") == 0) {
    huffman::huffman_encoder encoder(input_file, output_file);
    encoder.encode();
  } else if (strcmp(mode, "decode") == 0) {
    huffman::huffman_decoder decoder(input_file, output_file);
    decoder.decode();
  } else {
    return -1;
  }
  return 0;
}
