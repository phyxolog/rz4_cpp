/*
 * Copyright (C) 2018 Yura Zhivaga <yzhivaga@gmail.com>
 * 
 * This file is part of rz4.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "main.hpp"

using namespace std;
namespace fs = boost::filesystem;
namespace po = boost::program_options;

int usage() {
  cout << logo << usage_message << endl;
  return 1;
}

short int parse_args(Options &options, int argc, char *argv[]) {
  po::options_description desc("");
  desc.add_options()
    ("help,h", "Show help")
    ("wav,w", po::value<bool>())
    ("out,o", po::value<std::string>())
    ("outdir,d", po::value<std::string>())
    ("bufsize,b", po::value<std::string>());

  po::variables_map vm;
  po::parsed_options parsed = po::command_line_parser(argc, argv).options(desc).allow_unregistered().run();
  po::store(parsed, vm);
  po::notify(vm);

  if (vm.count("out")) {
    options.outfile = vm["out"].as<std::string>();
  }

  if (vm.count("outdir")) {
    options.outdir = vm["outdir"].as<std::string>();
  }

  if (vm.count("bufsize")) {
    options.buffer_size = memtoll(vm["bufsize"].as<std::string>());
  }

  if (vm.count("wav")) {
    options.enable_wav = vm["wav"].as<bool>();
  }

  if (vm.count("help")) {
    return 1;
  }

  return 0;
}

int main(int argc, char *argv[]) {
  if (argc < 3) {
    return usage();
  }

  // get current path
  const fs::path workdir = fs::current_path();

  // `command` always the first argument
  std::string command = argv[1];
  std::transform(command.begin(), command.end(), command.begin(), ::tolower);

  // init options
  Options options;
  options.command = command;
  options.buffer_size = BUFFER_SIZE;
  options.enable_wav = 1;

  // input file always the last arg
  options.infile = argv[argc - 1];

  Scan  *scanner;
  Eject *ejector;

  if (parse_args(options, argc, argv) == 1) {
    return usage();
  }

  cout << logo << endl;

  if (options.buffer_size <= 0) {
    options.buffer_size = BUFFER_SIZE;
  }

  if (options.infile.empty() || !fs::exists(options.infile)) {
    cout << "[!] No such input file!" << endl;
    return 1;
  }

  if (options.command == COMMAND_EXTRACT
      && options.outdir.empty()) {
    options.outdir = workdir / gennamep(options.infile, "extract_data");
  }

  if (options.command == COMMAND_EXTRACT
      && !fs::exists(options.outdir)) {
    fs::create_directory(options.outdir);
  }

  cout << "-> Buffer size: " << humnsize(options.buffer_size) << endl;

  scanner = new Scan(options);
  
  if (options.command == COMMAND_SCAN
      || options.command == COMMAND_COMPRESS
      || options.command == COMMAND_EXTRACT) {
    cout << "-> Scanning..." << endl << endl;

    scanner->run();
    scanner->close();

    cout
      << boost::format("--> Found %d signatures, total size: %d")
        % scanner->c_found_files()
        % humnsize(scanner->get_total_size())
      << endl;
  }

  if (options.command == COMMAND_EXTRACT) {
    cout << endl << "-> Extract data..." << endl;

    ejector = new Eject(options.infile, options.buffer_size);
    std::list<StreamInfo> stream_list(scanner->get_streamlist());

    uintmax_t i = 1, count = scanner->c_found_files();
    std::list<StreamInfo>::const_iterator iter, end; 
    
    for (iter = stream_list.begin(), end = stream_list.end(); iter != end; iter++, i++) {
      const fs::path path = options.outdir / boost::str(boost::format("%016X-%016X.%s")
                                            % (*iter).offset
                                            % (*iter).file_size
                                            % (*iter).ext);

      ejector->extract((*iter).offset, (*iter).file_size, path.string());
      std::cout << "\r" << "-> " << i * 100 / count << "% completed.";
    }

    ejector->close();

    cout << endl << "-> Extracting successfully!" << endl;
  }

  return 0;
}