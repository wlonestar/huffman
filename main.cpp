#include <getopt.h>

#include "huffman.hpp"

void print_version() {
  printf("huffman version(0.1.0)\n");
  printf("This is a command tool for compressing file simply using huffman coding\n");
}

void print_help() {
  printf("huffman version(0.1.0)\n\n");
  printf("Usage: huffman -m [mode] -i [input_file] -o [output_file]\n");
  printf("  -m --mode\t[encode|decode]\n");
  printf("     encode\tarchive file to specific archive\n");
  printf("     decode\textract a specific file from an archive\n");
  printf("  -v --version\tprint version information.\n");
  printf("  -h --help\tprint this information.\n");
}

const struct option table[] = {
  {"mode", required_argument, NULL, 'm'},
  {"input", required_argument, NULL, 'i'},
  {"output", required_argument, NULL, 'o'},
  {"version", no_argument, NULL, 'v'},
  {"help", no_argument, NULL, 'h'},
  {0, 0, NULL, 0},
};

void parse_args(int argc, char *argv[], std::string &mode,
                std::string &input_file, std::string &output_file) {
  opterr = 0;
  int o;
  while ((o = getopt_long(argc, argv, "m:i:o:vh", table, NULL)) != -1) {
    switch (o) {
      case 'm':
        mode = std::string(optarg);
        break;
      case 'i':
        input_file = std::string(optarg);
        break;
      case 'o':
        output_file = std::string(optarg);
        break;
      case 'v':
        print_version();
        exit(EXIT_SUCCESS);
      case 'h':
        print_help();
        exit(EXIT_SUCCESS);
      default:
        break;
    }
  }

  if (mode.compare("") == 0) {
    error("option requires an argument -- 'm'.");
    exit(EXIT_FAILURE);
  }
  if (mode.compare("encode") != 0 && mode.compare("decode") != 0) {
    error("mode is only 'encode' or 'decode'.");
    exit(EXIT_FAILURE);
  }
  if (input_file.compare("") == 0) {
    error("option requires an argument -- 'i'.");
    exit(EXIT_FAILURE);
  }
  if (output_file.compare("") == 0) {
    error("option requires an argument -- 'o'.");
    exit(EXIT_FAILURE);
  }
}

/**
 * huffman -m [encode|decode] -i [input_file] -o [output_file]
 */
int main(int argc, char *argv[]) {
  std::string mode = "";
  std::string input_file = "";
  std::string output_file = "";
  parse_args(argc, argv, mode, input_file, output_file);
  if (mode.compare("encode") == 0) {
    huffman::huffman_encoder encoder(std::move(input_file), std::move(output_file));
    encoder.encode();
  } else if (mode.compare("decode") == 0) {
    huffman::huffman_decoder decoder(std::move(input_file), std::move(output_file));
    decoder.decode();
  }
  return 0;
}
